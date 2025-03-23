#include <iostream>
#include <vector>
#include <cstring>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <ctime>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/rand.h>

/*
 * Compile with something like:
 *   g++ pkey.cpp -lcrypto -o pkey
 *
 * Usage:
 *   pkey ALGORITHM BIGINT
 *
 * ALGORITHM := ssh-ed25519 | ecdsa-sha2-nistp256 | ecdsa-sha2-nistp384 | ecdsa-sha2-nistp521
 * BIGINT can be in decimal or hex (hex if prefixed with 0x/0X).
 *
 * This generates an OpenSSH "openssh-key-v1" private key file on stdout.
 * Cipher/kdf are "none"; no passphrase encryption is performed.
 */

static void handleOpenSSLErrors() {
    ERR_print_errors_fp(stderr);
    exit(1);
}

static BIGNUM* parseBigInt(const std::string &bigStr) {
    // Detect hex vs dec
    BIGNUM *bn = BN_new();
    if(!bn) handleOpenSSLErrors();

    int rc;
    if(bigStr.size() > 2 && (bigStr[0] == '0') && (bigStr[1] == 'x' || bigStr[1] == 'X')) {
        // Hex
        rc = BN_hex2bn(&bn, bigStr.c_str() + 2);
    } else {
        // Decimal
        rc = BN_dec2bn(&bn, bigStr.c_str());
    }
    if(!rc) {
        BN_free(bn);
        throw std::runtime_error("Invalid BIGINT.");
    }
    return bn;
}

// Write a 32-bit integer (network byte order) into a buffer
static void writeUint32(std::vector<unsigned char> &buf, unsigned int val) {
    buf.push_back((val >> 24) & 0xff);
    buf.push_back((val >> 16) & 0xff);
    buf.push_back((val >> 8) & 0xff);
    buf.push_back(val & 0xff);
}

// Write an SSH-style "string": first uint32 length, then the raw bytes
static void writeString(std::vector<unsigned char> &buf, const unsigned char *data, size_t len) {
    writeUint32(buf, (unsigned int)len);
    buf.insert(buf.end(), data, data + len);
}
static void writeString(std::vector<unsigned char> &buf, const std::string &s) {
    writeUint32(buf, (unsigned int)s.size());
    buf.insert(buf.end(), s.begin(), s.end());
}

// Base64 encoding (simple fast approach)
static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64Encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve((len * 4 + 2) / 3);

    unsigned int val=0;
    int valb=-6;
    for(size_t i=0;i<len;i++){
        val=(val<<8)+data[i];
        valb+=8;
        while(valb>=0){
            out.push_back(b64chars[(val>>valb)&0x3F]);
            valb-=6;
        }
    }
    if(valb>-6) out.push_back(b64chars[((val<<8)>>(valb+8))&0x3F]);
    while(out.size()%4) out.push_back('=');
    return out;
}

// Convert BIGNUM to binary (big-endian). Output has no leading zero bytes (OpenSSH typically omits them).
static std::vector<unsigned char> bnToBinary(const BIGNUM *bn) {
    int len = BN_num_bytes(bn);
    std::vector<unsigned char> buf(len);
    BN_bn2bin(bn, &buf[0]);
    return buf;
}

// For ECDSA: write the "string" of the uncompressed public key for the new-key-format
// This function returns the uncompressed EC point as a string-block.
static std::vector<unsigned char> ecPointToString(EC_KEY *ec) {
    EC_GROUP *grp = (EC_GROUP*)EC_KEY_get0_group(ec);
    EC_POINT *pt = (EC_POINT*)EC_KEY_get0_public_key(ec);
    if(!grp || !pt) throw std::runtime_error("EC_KEY missing group or pubkey.");

    // Convert to uncompressed form
    size_t outLen = EC_POINT_point2oct(grp, pt, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
    if(!outLen) throw std::runtime_error("EC_POINT_point2oct failed.");
    std::vector<unsigned char> out(outLen);
    if(!EC_POINT_point2oct(grp, pt, POINT_CONVERSION_UNCOMPRESSED, &out[0], outLen, NULL))
        throw std::runtime_error("EC_POINT_point2oct failed(2).");

    return out;
}

// Wrangle an EC_KEY from group NID and private BIGNUM
static EC_KEY* createECKey(int nid, BIGNUM *priv) {
    EC_KEY *ec = EC_KEY_new_by_curve_name(nid);
    if(!ec) handleOpenSSLErrors();

    if(!EC_KEY_set_private_key(ec, priv)) {
        EC_KEY_free(ec);
        throw std::runtime_error("EC_KEY_set_private_key failed.");
    }
    // Derive public key
    const EC_GROUP *grp = EC_KEY_get0_group(ec);
    EC_POINT *pub=EC_POINT_new(grp);
    if(!pub) {
        EC_KEY_free(ec);
        throw std::runtime_error("EC_POINT_new failed.");
    }
    if(!EC_POINT_mul(grp, pub, priv, NULL, NULL, NULL)) {
        EC_POINT_free(pub);
        EC_KEY_free(ec);
        throw std::runtime_error("EC_POINT_mul failed.");
    }
    if(!EC_KEY_set_public_key(ec, pub)) {
        EC_POINT_free(pub);
        EC_KEY_free(ec);
        throw std::runtime_error("EC_KEY_set_public_key failed.");
    }
    EC_POINT_free(pub);

    if(!EC_KEY_check_key(ec)) {
        EC_KEY_free(ec);
        throw std::runtime_error("EC_KEY_check_key failed.");
    }
    return ec;
}

// For ED25519, we want a 32-byte seed. We'll do BN mod 2^251 - 27742317777372353535851937790883648493?
// Actually, let's do a simpler approach: take the BN as big-endian, then pad/truncate to 32 bytes. 
static void bnToEd25519Seed(const BIGNUM *bn, unsigned char out[32]) {
    // zero out
    std::memset(out, 0, 32);

    // BN to big-endian
    int nbytes = BN_num_bytes(bn);
    std::vector<unsigned char> tmp(nbytes);
    BN_bn2bin(bn, &tmp[0]);

    // If bigger than 32, keep the rightmost 32 bytes
    // i.e. we preserve the least significant 32 bytes in big-endian sense, so effectively we drop leading.
    // We'll just copy from the end of tmp into the end of out.
    if(nbytes <= 32) {
        // put entire tmp at the end of out
        std::memcpy(out + (32 - nbytes), &tmp[0], nbytes);
    } else {
        // only copy the last 32
        std::memcpy(out, &tmp[nbytes - 32], 32);
    }
}

int main(int argc, char** argv) {
    if(argc != 3) {
        std::cerr << std::endl << "Usage: pkey ALGORITHM BIGINT" << std::endl << std::endl;
		std::cerr << "ALGORITHM := 'ssh-ed25519' | 'ecdsa-sha2-nistp{256,384,521}'" << std::endl;
		std::cerr << "BIGINT can either be specified in DECIMAL, or in HEXIDECIMAL with '0x' prefix" << std::endl << std::endl;
        return 1;
    }

    std::string sshAlg = argv[1];
    std::string bigStr = argv[2];

    OpenSSL_add_all_algorithms();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();

    // Parse the private integer
    BIGNUM *privBN = nullptr;
    try {
        privBN = parseBigInt(bigStr);
    } catch(std::exception &e) {
        std::cerr << "Error parsing BIGINT: " << e.what() << "\n";
        return 1;
    }

    // Build the "openssh-key-v1" binary structure
    // ------------------------------------------------
    std::vector<unsigned char> plain; 
    // "checkint1" and "checkint2" - random 32-bit
    srand((unsigned int)time(NULL));
    unsigned int checkInt = ((unsigned int)rand() << 1) ^ (unsigned int)rand();
    writeUint32(plain, checkInt);
    writeUint32(plain, checkInt);

    // The algorithm string (for the private portion) is literally the SSH name
    std::string curveName; // for ecdsa
    int nid = 0;

    bool isEd25519 = false;
    bool isEcdsa = false;

    if(sshAlg == "ssh-ed25519") {
        isEd25519 = true;
    } else if(sshAlg == "ecdsa-sha2-nistp256") {
        curveName = "nistp256";
        nid = NID_X9_62_prime256v1;
        isEcdsa = true;
    } else if(sshAlg == "ecdsa-sha2-nistp384") {
        curveName = "nistp384";
        nid = NID_secp384r1;
        isEcdsa = true;
    } else if(sshAlg == "ecdsa-sha2-nistp521") {
        curveName = "nistp521";
        nid = NID_secp521r1;
        isEcdsa = true;
    } else {
        std::cerr << "Unknown ALGORITHM.\n";
        BN_free(privBN);
        return 1;
    }

    // We must also build the public key so we can store it in the "public" half and the "private" half.
    // For ED25519, we do EVP_PKEY_new_raw_private_key, then EVP_PKEY_get_raw_public_key.
    // For ECDSA, we do createECKey, then get uncompressed pub, etc.

    std::vector<unsigned char> pubKeyStr; 
    std::vector<unsigned char> privKeyStr; 
    // For building the "private" portion: we do:
    //   writeString(plain, sshAlg)
    //   (for ECDSA) writeString(plain, curveName)
    //   writeString(plain, pubKey)   // same pub as in the outer public block
    //   // then the actual private scalar or (ed25519 private+public)
    //   // for ecdsa: BN
    //   // for ed25519: 32 bytes priv + 32 bytes pub

    if(isEd25519) {
        // Convert BN to 32 byte seed
        unsigned char seed[32];
        bnToEd25519Seed(privBN, seed);

        EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, seed, 32);
        if(!pkey) {
            BN_free(privBN);
            throw std::runtime_error("EVP_PKEY_new_raw_private_key(ED25519) failed.");
        }
        unsigned char pubRaw[32];
        size_t pubLen = 32;
        if(!EVP_PKEY_get_raw_public_key(pkey, pubRaw, &pubLen) || pubLen!=32) {
            EVP_PKEY_free(pkey);
            BN_free(privBN);
            throw std::runtime_error("EVP_PKEY_get_raw_public_key(ED25519) failed.");
        }
        // Build "public key" string:  (algorithm) + raw pub
        // For the top-level public key portion, it's: string(sshAlg), string(pubRaw)
        // But in the "ssh-ed25519" format, there's only one string for the pub inside: 32 bytes.
        // So let's keep a separate buffer for the final public block.

        // The new-key-format public portion is:
        //   string(ssh-ed25519),
        //   string(pubRaw (32 bytes)).
        {
            // We'll fill pubKeyStr with "ssh-ed25519" => pubRaw
            // This is the chunk to store in the top-level public part.
            std::vector<unsigned char> tmp;
            writeString(tmp, sshAlg);
            writeString(tmp, (unsigned char*)pubRaw, 32);
            pubKeyStr = tmp;
        }
        {
            // For private portion: 
            //   string(sshAlg)
            //   string(pubRaw)
            //   string( (32 bytes priv seed) + (32 bytes pub) )
            writeString(plain, sshAlg);
            {
                // pub portion
                writeString(plain, (unsigned char*)pubRaw, 32);
            }
            {
                // private + pub
                std::vector<unsigned char> combo(64);
                std::memcpy(&combo[0], seed, 32);
                std::memcpy(&combo[32], pubRaw, 32);
                writeString(plain, combo.data(), 64);
            }
        }

        EVP_PKEY_free(pkey);
    } 
    else if(isEcdsa) {
        EC_KEY *ec = createECKey(nid, privBN);
        // Public key uncompressed
        std::vector<unsigned char> pubBin = ecPointToString(ec);

        // For top-level public:
        //   string(sshAlg),
        //   string(curveName),
        //   string(pubBin)
        {
            std::vector<unsigned char> tmp;
            writeString(tmp, sshAlg);
            writeString(tmp, curveName);
            writeString(tmp, pubBin.data(), pubBin.size());
            pubKeyStr = tmp;
        }

        // For private portion inside the "plain":
        //   string(sshAlg)
        //   string(curveName)
        //   string(pubBin)
        //   mpi of priv
        {
            writeString(plain, sshAlg);
            writeString(plain, curveName);
            writeString(plain, pubBin.data(), pubBin.size());

            // Then the private scalar (BN) as a string:
            auto dBin = bnToBinary(privBN);
            writeString(plain, dBin.data(), dBin.size());
        }

        EC_KEY_free(ec);
    }

    // Now we build the full key structure to be base64-encoded:
    // "openssh-key-v1\0" + cipher/kdf = "none","none","", plus #keys=1, plus the public key(s), plus the private block above.
    std::vector<unsigned char> finalBlob;

    // "openssh-key-v1" plus NUL
    {
        static const char magic[] = "openssh-key-v1";
        for(size_t i=0; i<sizeof(magic)-1; i++) {
            finalBlob.push_back(magic[i]);
        }
        finalBlob.push_back('\0');
    }

    // ciphername => "none"
    {
        std::string s("none");
        writeString(finalBlob, s);
    }
    // kdfname => "none"
    {
        std::string s("none");
        writeString(finalBlob, s);
    }
    // kdfoptions => ""
    {
        std::string s("");
        writeString(finalBlob, s);
    }
    // number_of_keys => 1
    writeUint32(finalBlob, 1);

    // public keys => 1
    //   the entire previously built pubKeyStr as a "string"
    writeString(finalBlob, pubKeyStr.data(), pubKeyStr.size());

    // private keys => the 'plain' array
    //   we must store it as 1 big "string"
    writeString(finalBlob, plain.data(), plain.size());

    // Base64 encode finalBlob and print in OpenSSH key style
    std::string b64 = base64Encode(finalBlob.data(), finalBlob.size());

    std::cout << "-----BEGIN OPENSSH PRIVATE KEY-----\n";
    // Print 70 chars per line:
    for(size_t i=0; i<b64.size(); i+=70) {
        std::cout << b64.substr(i, 70) << "\n";
    }
    std::cout << "-----END OPENSSH PRIVATE KEY-----\n";

    BN_free(privBN);
    return 0;
}
