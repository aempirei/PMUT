#include <iostream>
#include <cstdlib>
#include <cstring>
#include <string>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

// Helper function to parse the input string into a BIGNUM.
// Supports:
//   - Hexadecimal if the string starts with "0x" or "0X"
//   - Binary if the string starts with "0b" or "0B"
//   - Octal if the string starts with '0' and has more than one digit
//   - Decimal otherwise
BIGNUM* parse_bn(const char* str) {
    if (!str || !*str) return nullptr;
    size_t len = std::strlen(str);
    // Hexadecimal
    if (len > 2 && str[0]=='0' && (str[1]=='x' || str[1]=='X')) {
        BIGNUM* bn = nullptr;
        if (BN_hex2bn(&bn, str+2) == 0)
            return nullptr;
        return bn;
    }
    // Binary
    else if (len > 2 && str[0]=='0' && (str[1]=='b' || str[1]=='B')) {
        BIGNUM* bn = BN_new();
        BN_zero(bn);
        for (size_t i = 2; i < len; i++) {
            char c = str[i];
            if (c != '0' && c != '1') { BN_free(bn); return nullptr; }
            if (!BN_lshift(bn, bn, 1)) { BN_free(bn); return nullptr; }
            if (c == '1') {
                if (!BN_add_word(bn, 1)) { BN_free(bn); return nullptr; }
            }
        }
        return bn;
    }
    // Octal (if first char is '0' and length > 1)
    else if (str[0]=='0' && len > 1) {
        BIGNUM* bn = BN_new();
        BN_zero(bn);
        for (size_t i = 0; i < len; i++) {
            char c = str[i];
            if (c < '0' || c > '7') { BN_free(bn); return nullptr; }
            if (!BN_mul_word(bn, 8)) { BN_free(bn); return nullptr; }
            if (!BN_add_word(bn, c - '0')) { BN_free(bn); return nullptr; }
        }
        return bn;
    }
    // Decimal
    else {
        BIGNUM* bn = nullptr;
        if (BN_dec2bn(&bn, str) == 0)
            return nullptr;
        return bn;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " {ecdsa | ed25519} integer" << std::endl;
        return EXIT_FAILURE;
    }

    std::string algo = argv[1];
    BIGNUM* bn = parse_bn(argv[2]);
    if (!bn) {
        std::cerr << "Error: could not parse integer input." << std::endl;
        return EXIT_FAILURE;
    }

    if (algo == "ecdsa") {
        // Create an ECDSA key on the NIST P-256 (prime256v1) curve.
        int nid = NID_X9_62_prime256v1;
        EC_KEY* ec_key = EC_KEY_new_by_curve_name(nid);
        if (!ec_key) {
            std::cerr << "Error creating EC_KEY." << std::endl;
            BN_free(bn);
            return EXIT_FAILURE;
        }

        // Prepare a BN_CTX for computations.
        BN_CTX* ctx = BN_CTX_new();
        if (!ctx) {
            std::cerr << "Error creating BN_CTX." << std::endl;
            EC_KEY_free(ec_key);
            BN_free(bn);
            return EXIT_FAILURE;
        }

        // Make sure the private key is in the valid range [1, order-1].
        const EC_GROUP* group = EC_KEY_get0_group(ec_key);
        BIGNUM* order = BN_new();
        if (!EC_GROUP_get_order(group, order, ctx)) {
            std::cerr << "Error getting group order." << std::endl;
            BN_free(bn);
            EC_KEY_free(ec_key);
            BN_CTX_free(ctx);
            return EXIT_FAILURE;
        }
        BIGNUM* one = BN_new();
        BN_one(one);
        BIGNUM* mod = BN_new();
        BN_sub(mod, order, one);  // mod = order - 1
        BIGNUM* priv = BN_new();
        BN_mod(priv, bn, mod, ctx);
        BN_add(priv, priv, one);  // now in [1, order-1]

        // Set the private key.
        if (!EC_KEY_set_private_key(ec_key, priv)) {
            std::cerr << "Error setting private key." << std::endl;
            BN_free(bn); BN_free(order); BN_free(one); BN_free(mod); BN_free(priv);
            EC_KEY_free(ec_key);
            BN_CTX_free(ctx);
            return EXIT_FAILURE;
        }

        // Compute the corresponding public key: pub = priv * generator.
        EC_POINT* pub_key = EC_POINT_new(group);
        if (!EC_POINT_mul(group, pub_key, priv, NULL, NULL, ctx)) {
            std::cerr << "Error computing public key." << std::endl;
            BN_free(bn); BN_free(order); BN_free(one); BN_free(mod); BN_free(priv);
            EC_POINT_free(pub_key);
            EC_KEY_free(ec_key);
            BN_CTX_free(ctx);
            return EXIT_FAILURE;
        }
        if (!EC_KEY_set_public_key(ec_key, pub_key)) {
            std::cerr << "Error setting public key." << std::endl;
            BN_free(bn); BN_free(order); BN_free(one); BN_free(mod); BN_free(priv);
            EC_POINT_free(pub_key);
            EC_KEY_free(ec_key);
            BN_CTX_free(ctx);
            return EXIT_FAILURE;
        }

        // Output the key in PEM format to stdout.
        if (!PEM_write_ECPrivateKey(stdout, ec_key, nullptr, nullptr, 0, nullptr, nullptr)) {
            std::cerr << "Error writing EC private key to stdout." << std::endl;
        }

        // Cleanup.
        BN_free(bn);
        BN_free(order);
        BN_free(one);
        BN_free(mod);
        BN_free(priv);
        EC_POINT_free(pub_key);
        EC_KEY_free(ec_key);
        BN_CTX_free(ctx);
    }
    else if (algo == "ed25519") {
        // For ed25519, the private key is a 32-byte seed.
        // We reduce the input integer modulo 2^256.
        BN_CTX* ctx = BN_CTX_new();
        if (!ctx) {
            std::cerr << "Error creating BN_CTX." << std::endl;
            BN_free(bn);
            return EXIT_FAILURE;
        }
        BIGNUM* mod2 = BN_new();
        BIGNUM* one = BN_new();
        BN_one(mod2);
        BN_lshift(mod2, mod2, 256);  // mod2 = 2^256
        BIGNUM* priv_bn = BN_new();
        BN_mod(priv_bn, bn, mod2, ctx);

        // Convert the BIGNUM to a 32-byte (big-endian) buffer.
        unsigned char seed[32] = {0};
        int num_bytes = BN_num_bytes(priv_bn);
        if (num_bytes > 32) {
            std::cerr << "Error: private key value too large after reduction." << std::endl;
            BN_free(bn); BN_free(mod2); BN_free(one); BN_free(priv_bn);
            BN_CTX_free(ctx);
            return EXIT_FAILURE;
        }
        // BN_bn2bin outputs in big-endian order; pad on the left if necessary.
        BN_bn2bin(priv_bn, seed + (32 - num_bytes));

        // Create an EVP_PKEY for ed25519 using the raw seed.
        EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed, 32);
        if (!pkey) {
            std::cerr << "Error creating ed25519 key." << std::endl;
            BN_free(bn); BN_free(mod2); BN_free(one); BN_free(priv_bn);
            BN_CTX_free(ctx);
            return EXIT_FAILURE;
        }

        // Output the private key in PEM format (PKCS#8) to stdout.
        if (!PEM_write_PrivateKey(stdout, pkey, nullptr, nullptr, 0, nullptr, nullptr)) {
            std::cerr << "Error writing ed25519 private key to stdout." << std::endl;
        }

        // Cleanup.
        BN_free(bn);
        BN_free(mod2);
        BN_free(one);
        BN_free(priv_bn);
        EVP_PKEY_free(pkey);
        BN_CTX_free(ctx);
    }
    else {
        std::cerr << "Usage: " << argv[0] << " {ecdsa | ed25519} integer" << std::endl;
        BN_free(bn);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

