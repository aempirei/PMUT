#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/err.h>

static void handleOpenSSLErrors() {
    ERR_print_errors_fp(stderr);
    exit(1);
}

static BIGNUM* parseBigInt(const std::string &bigStr) {
    BIGNUM *bn = BN_new();
    if (!bn) handleOpenSSLErrors();

    if (bigStr.size() > 2 && (bigStr[0] == '0') && (bigStr[1] == 'x' || bigStr[1] == 'X')) {
        if (!BN_hex2bn(&bn, bigStr.c_str() + 2)) {
            BN_free(bn);
            throw std::runtime_error("Invalid hex BIGINT.");
        }
    } else {
        if (!BN_dec2bn(&bn, bigStr.c_str())) {
            BN_free(bn);
            throw std::runtime_error("Invalid decimal BIGINT.");
        }
    }
    return bn;
}

static int curveSizeToNid(int curveSize) {
    switch(curveSize) {
        case 256: return NID_X9_62_prime256v1;
        case 384: return NID_secp384r1;
        case 521: return NID_secp521r1;
        default:
            throw std::runtime_error("Invalid curve size. Must be 256|384|521.");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: peck {256|384|521} BIGINT\n";
        return 1;
    }

    int curveSize = std::stoi(argv[1]);
    std::string bigStr = argv[2];

    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();

    try {
        int nid = curveSizeToNid(curveSize);
        BIGNUM *privBN = parseBigInt(bigStr);

        // Create key
        EC_KEY* ecKey = EC_KEY_new_by_curve_name(nid);
        if(!ecKey) handleOpenSSLErrors();

        // Set private key
        if (!EC_KEY_set_private_key(ecKey, privBN)) {
            BN_free(privBN);
            EC_KEY_free(ecKey);
            throw std::runtime_error("EC_KEY_set_private_key failed.");
        }

        // Derive public key
        const EC_GROUP *grp = EC_KEY_get0_group(ecKey);
        EC_POINT *pub=EC_POINT_new(grp);
        if(!pub) {
            BN_free(privBN);
            EC_KEY_free(ecKey);
            throw std::runtime_error("EC_POINT_new failed.");
        }
        if(!EC_POINT_mul(grp, pub, privBN, NULL, NULL, NULL)) {
            EC_POINT_free(pub);
            BN_free(privBN);
            EC_KEY_free(ecKey);
            throw std::runtime_error("EC_POINT_mul failed.");
        }
        if(!EC_KEY_set_public_key(ecKey, pub)) {
            EC_POINT_free(pub);
            BN_free(privBN);
            EC_KEY_free(ecKey);
            throw std::runtime_error("EC_KEY_set_public_key failed.");
        }
        EC_POINT_free(pub);

        // Write EC private key in SEC1 PEM format
        if (!PEM_write_ECPrivateKey(stdout, ecKey, NULL, NULL, 0, NULL, NULL)) {
            BN_free(privBN);
            EC_KEY_free(ecKey);
            handleOpenSSLErrors();
        }

        BN_free(privBN);
        EC_KEY_free(ecKey);
    }
    catch(std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}