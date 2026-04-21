/*
 * Copyright (C) 2024 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 */

/* This file includes AI-generated code - Review and modify as needed */

/*
 * oqs_qkd_etsi_api_wrapper.h
 * Wrapper for QKD ETSI API to maintain compatibility with existing QKD_CTX
 * interface
 */

#ifndef QKD_ETSI_API_WRAPPER_H_
#define QKD_ETSI_API_WRAPPER_H_

#if !defined(ETSI_004_API) && !defined(ETSI_014_API)
#define ETSI_014_API
#endif

#include <openssl/evp.h>
#include <qkd-etsi-api-c-wrapper/qkd_config.h>
#include <qkd-etsi-api-c-wrapper/qkd_etsi_api.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef ETSI_004_API
#include <qkd-etsi-api-c-wrapper/etsi004/api.h>
#elif defined(ETSI_014_API)
#include <qkd-etsi-api-c-wrapper/etsi014/api.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Main QKD context structure */
typedef struct {
#ifdef ETSI_004_API
    unsigned char key_id[16];
#elif defined(ETSI_014_API)
    unsigned char key_id[37]; // 36 chars for UUID + null terminator
#endif
    
    // Self information (new fields - always used)
    char *my_kme_hostname;    // This node's KME
    char *my_sae_id;          // This node's SAE identifier
    
    // Peer information
    char *peer_sae_id;        // Peer's SAE identifier
    
    // Legacy fields (for backward compatibility during transition)
    char *source_uri;         // URI for source KME (ETSI 004) or points to my_kme_hostname (ETSI 014)
    char *dest_uri;           // URI for destination KME (unused in ETSI 014)
    char *sae_id;             // Points to peer_sae_id
    
    // Deprecated fields (will be removed in future version)
    char *master_kme;         // DEPRECATED: Use my_kme_hostname
    char *slave_kme;          // DEPRECATED: Use my_kme_hostname
    char *master_sae;         // DEPRECATED: Use my_sae_id or peer_sae_id
    char *slave_sae;          // DEPRECATED: Use my_sae_id or peer_sae_id
    
    EVP_PKEY *key;
    bool is_initiator;        // Transient role: true for initiator, false for responder
    
#ifdef ETSI_004_API
    bool is_connected;
    struct qkd_qos_s qos;
    struct qkd_metadata_s metadata;
#elif defined(ETSI_014_API)
    qkd_status_t status;
#endif
} QKD_CTX;

#ifdef ETSI_004_API
/* ETSI API Wrapper Functions - maintaining original function signatures */
bool qkd_open(QKD_CTX *ctx);
bool qkd_close(QKD_CTX *ctx);
bool qkd_get_key(QKD_CTX *ctx);
#endif /* ETSI_004_API */
#ifdef ETSI_014_API
bool qkd_get_status(QKD_CTX *ctx);
bool qkd_get_key(QKD_CTX *ctx);
bool qkd_get_key_with_ids(QKD_CTX *ctx);
#endif /* ETSI_014_API */

#ifdef __cplusplus
}
#endif

#endif /* QKD_ETSI_API_WRAPPER_H_ */