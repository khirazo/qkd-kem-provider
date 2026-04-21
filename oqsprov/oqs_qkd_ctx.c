/*
 * Copyright (C) 2024 Javier Blanco-Romero @fj-blanco (UC3M, QURSA project)
 */

/* This file includes AI-generated code - Review and modify as needed */

/*
 * oqs_qkd_ctx.c
 * QKD context initialization
 */
#include "oqs_qkd_ctx.h"

// #define DEBUG_QKD

#ifdef NDEBUG
#define OQS_KEY_PRINTF(a)
#define OQS_KEY_PRINTF2(a, b)
#define OQS_KEY_PRINTF3(a, b, c)
#define QKD_DEBUG(fmt, ...)
#else
#define OQS_KEY_PRINTF(a)                                                      \
    if (getenv("OQSKEY"))                                                      \
    printf(a)
#define OQS_KEY_PRINTF2(a, b)                                                  \
    if (getenv("OQSKEY"))                                                      \
    printf(a, b)
#define OQS_KEY_PRINTF3(a, b, c)                                               \
    if (getenv("OQSKEY"))                                                      \
    printf(a, b, c)
#ifdef DEBUG_QKD
#define QKD_DEBUG(fmt, ...)                                                    \
    fprintf(stderr, "QKD DEBUG: %s:%d: " fmt "\n", __func__, __LINE__,         \
            ##__VA_ARGS__)
#else
#define QKD_DEBUG(fmt, ...)
#endif
#endif // NDEBUG

/**
 * Helper function to get environment variable with fallback support
 */
static const char* get_env_with_fallback(
    const char *new_var,
    const char *old_var1,
    const char *old_var2,
    bool *used_deprecated
) {
    const char *value = getenv(new_var);
    if (value) {
        return value;
    }
    
    value = getenv(old_var1);
    if (value) {
        *used_deprecated = true;
        return value;
    }
    
    if (old_var2) {
        value = getenv(old_var2);
        if (value) {
            *used_deprecated = true;
            return value;
        }
    }
    
    return NULL;
}

/**
 * Print deprecation warning once per process
 */
static void print_deprecation_warning_once(void) {
    static bool warning_printed = false;
    if (!warning_printed) {
        fprintf(stderr,
            "WARNING: QKD_MASTER_*/QKD_SLAVE_* environment variables are deprecated.\n"
            "         Please migrate to QKD_MY_* variables.\n"
            "         See qkd-kem-provider/docs/ARCHITECTURE-REFACTORING.md for details.\n");
        warning_printed = true;
    }
}

/**
 * Initialize QKD URIs and identifiers from environment variables
 */
static int qkd_init_uris(QKD_CTX *ctx) {
    if (!ctx) {
        QKD_DEBUG("Invalid context");
        return OQS_ERROR;
    }

#ifdef ETSI_004_API
    // ETSI 004: Use direct URIs (no change needed)
    const char *source_uri = getenv("QKD_SOURCE_URI");
    const char *dest_uri = getenv("QKD_DEST_URI");

    if (!source_uri || !dest_uri) {
        QKD_DEBUG("ETSI 004: QKD_SOURCE_URI and QKD_DEST_URI must be set");
        return OQS_ERROR;
    }

    QKD_DEBUG("ETSI 004: Using URIs from environment: source=%s, dest=%s",
              source_uri, dest_uri);

    ctx->source_uri = OPENSSL_strdup(source_uri);
    ctx->dest_uri = OPENSSL_strdup(dest_uri);

    if (!ctx->source_uri || !ctx->dest_uri) {
        QKD_DEBUG("ETSI 004: Failed to allocate URI strings");
        goto err;
    }

#else
    // ETSI 014: Use new MY_*/PEER_* variables with fallback
    bool used_deprecated = false;
    
    // Load MY_KME_HOSTNAME
    const char *my_kme = get_env_with_fallback(
        "QKD_MY_KME_HOSTNAME",
        "QKD_MASTER_KME_HOSTNAME",
        "QKD_SLAVE_KME_HOSTNAME",
        &used_deprecated
    );
    
    // Load MY_SAE_ID
    const char *my_sae = get_env_with_fallback(
        "QKD_MY_SAE_ID",
        "QKD_MASTER_SAE",
        "QKD_SLAVE_SAE",
        &used_deprecated
    );
    
    // Load PEER_SAE_ID
    const char *peer_sae = get_env_with_fallback(
        "QKD_PEER_SAE_ID",
        ctx->is_initiator ? "QKD_SLAVE_SAE" : "QKD_MASTER_SAE",
        NULL,
        &used_deprecated
    );
    
    // Print deprecation warning if needed
    if (used_deprecated) {
        print_deprecation_warning_once();
    }
    
    // Validate required variables
    if (!my_kme || !my_sae || !peer_sae) {
        QKD_DEBUG("ETSI 014: Required environment variables not set");
        QKD_DEBUG("Required: QKD_MY_KME_HOSTNAME, QKD_MY_SAE_ID, QKD_PEER_SAE_ID");
        return OQS_ERROR;
    }

    QKD_DEBUG("ETSI 014: Using configuration: my_kme=%s, my_sae=%s, peer_sae=%s",
              my_kme, my_sae, peer_sae);

    // Store in new fields
    ctx->my_kme_hostname = OPENSSL_strdup(my_kme);
    ctx->my_sae_id = OPENSSL_strdup(my_sae);
    ctx->peer_sae_id = OPENSSL_strdup(peer_sae);

    if (!ctx->my_kme_hostname || !ctx->my_sae_id || !ctx->peer_sae_id) {
        QKD_DEBUG("ETSI 014: Failed to allocate strings");
        goto err;
    }

    // Set legacy pointers for backward compatibility
    // Both initiator and responder use the same logic now
    ctx->source_uri = ctx->my_kme_hostname;  // Always my KME
    ctx->sae_id = ctx->peer_sae_id;          // Always peer SAE
    ctx->dest_uri = NULL;                     // Not used in ETSI 014
    
    // Also populate deprecated fields for any code that still uses them
    ctx->master_kme = ctx->my_kme_hostname;
    ctx->slave_kme = ctx->my_kme_hostname;
    ctx->master_sae = ctx->is_initiator ? ctx->my_sae_id : ctx->peer_sae_id;
    ctx->slave_sae = ctx->is_initiator ? ctx->peer_sae_id : ctx->my_sae_id;
#endif

    QKD_DEBUG("Final configuration: source_uri=%s, sae_id=%s",
              ctx->source_uri ? ctx->source_uri : "NULL",
              ctx->sae_id ? ctx->sae_id : "NULL");

    return OQS_SUCCESS;

err:
#ifdef ETSI_004_API
    if (ctx->source_uri) { OPENSSL_free(ctx->source_uri); ctx->source_uri = NULL; }
    if (ctx->dest_uri) { OPENSSL_free(ctx->dest_uri); ctx->dest_uri = NULL; }
#else
    if (ctx->my_kme_hostname) { OPENSSL_free(ctx->my_kme_hostname); ctx->my_kme_hostname = NULL; }
    if (ctx->my_sae_id) { OPENSSL_free(ctx->my_sae_id); ctx->my_sae_id = NULL; }
    if (ctx->peer_sae_id) { OPENSSL_free(ctx->peer_sae_id); ctx->peer_sae_id = NULL; }
    ctx->source_uri = NULL;
    ctx->dest_uri = NULL;
    ctx->sae_id = NULL;
    ctx->master_kme = NULL;
    ctx->slave_kme = NULL;
    ctx->master_sae = NULL;
    ctx->slave_sae = NULL;
#endif
    return OQS_ERROR;
}

int oqs_init_qkd_context(OQSX_KEY *oqsx_key, bool is_initiator) {
    int ret = OQS_SUCCESS;

    QKD_DEBUG("Initializing QKD context");

    // Return success if context already exists and role matches
    if (oqsx_key->qkd_ctx != NULL) {
#ifdef ETSI_014_API
#ifdef DEBUG_QKD
        if (!qkd_get_status(oqsx_key->qkd_ctx)) {
            QKD_DEBUG("Failed to get QKD status for existing context");
            return OQS_ERROR;
        }
#endif /* NDEBUG */
#endif

        if (oqsx_key->qkd_ctx->is_initiator == is_initiator) {
            QKD_DEBUG("QKD context already initialized with correct role");
            return OQS_SUCCESS;
        }

        if (is_initiator && oqsx_key->comp_privkey) {
            QKD_DEBUG("QKD context exists with key - preserving for initiator");
            oqsx_key->qkd_ctx->is_initiator = true;
            return OQS_SUCCESS;
        }

        OPENSSL_free(oqsx_key->qkd_ctx);
        oqsx_key->qkd_ctx = NULL;
    }

    // Allocate and initialize new context
    oqsx_key->qkd_ctx = OPENSSL_malloc(sizeof(QKD_CTX));
    if (oqsx_key->qkd_ctx == NULL) {
        QKD_DEBUG("Failed to allocate QKD context");
        return OQS_ERROR;
    }

    // Initialize context with clean state
    memset(oqsx_key->qkd_ctx, 0, sizeof(QKD_CTX));
    oqsx_key->qkd_ctx->is_initiator = is_initiator;

    // Initialize URIs - this must be done before status check
    ret = qkd_init_uris(oqsx_key->qkd_ctx);
    if (ret != OQS_SUCCESS) {
        QKD_DEBUG("Failed to initialize QKD URIs");
        OPENSSL_free(oqsx_key->qkd_ctx);
        oqsx_key->qkd_ctx = NULL;
        return OQS_ERROR;
    }

#ifdef ETSI_004_API
    /* Initialize QoS parameters for ETSI 004 */
    
    // Define metadata max size if not already defined
    #ifndef QKD_METADATA_MAX_SIZE
    #define QKD_METADATA_MAX_SIZE 1024
    #endif
    
    // Set default QoS parameters using the correct field names (lowercase for some)
    oqsx_key->qkd_ctx->qos.Key_chunk_size = QKD_KEY_SIZE;
    oqsx_key->qkd_ctx->qos.Timeout = 60000;
    oqsx_key->qkd_ctx->qos.Priority = 0;
    oqsx_key->qkd_ctx->qos.Max_bps = 40000;
    oqsx_key->qkd_ctx->qos.Min_bps = 5000;
    oqsx_key->qkd_ctx->qos.Jitter = 10;
    oqsx_key->qkd_ctx->qos.TTL = 3600;
    strcpy(oqsx_key->qkd_ctx->qos.Metadata_mimetype, "application/json");
    
    if (!oqsx_key->qkd_ctx->qos.Metadata_mimetype) {
        QKD_DEBUG("Memory allocation for metadata mimetype failed");
        OPENSSL_free(oqsx_key->qkd_ctx);
        oqsx_key->qkd_ctx = NULL;
        return OQS_ERROR;
    }
    
    // Override with environment variables if present
    const char *env_chunk_size = getenv("QKD_QOS_KEY_CHUNK_SIZE");
    const char *env_timeout = getenv("QKD_QOS_TIMEOUT");
    const char *env_priority = getenv("QKD_QOS_PRIORITY");
    const char *env_max_bps = getenv("QKD_QOS_MAX_BPS");
    const char *env_min_bps = getenv("QKD_QOS_MIN_BPS");
    const char *env_jitter = getenv("QKD_QOS_JITTER");
    const char *env_ttl = getenv("QKD_QOS_TTL");
    
    if (env_chunk_size) oqsx_key->qkd_ctx->qos.Key_chunk_size = atoi(env_chunk_size);
    if (env_timeout) oqsx_key->qkd_ctx->qos.Timeout = atoi(env_timeout);
    if (env_priority) oqsx_key->qkd_ctx->qos.Priority = atoi(env_priority);
    if (env_max_bps) oqsx_key->qkd_ctx->qos.Max_bps = atoi(env_max_bps);
    if (env_min_bps) oqsx_key->qkd_ctx->qos.Min_bps = atoi(env_min_bps);
    if (env_jitter) oqsx_key->qkd_ctx->qos.Jitter = atoi(env_jitter);
    if (env_ttl) oqsx_key->qkd_ctx->qos.TTL = atoi(env_ttl);
    
    // Initialize metadata structure
    oqsx_key->qkd_ctx->metadata.Metadata_size = QKD_METADATA_MAX_SIZE;
    oqsx_key->qkd_ctx->metadata.Metadata_buffer = OPENSSL_malloc(QKD_METADATA_MAX_SIZE);
    if (!oqsx_key->qkd_ctx->metadata.Metadata_buffer) {
        QKD_DEBUG("Memory allocation for metadata buffer failed");
        OPENSSL_free(oqsx_key->qkd_ctx->qos.Metadata_mimetype);
        OPENSSL_free(oqsx_key->qkd_ctx);
        oqsx_key->qkd_ctx = NULL;
        return OQS_ERROR;
    }
    memset(oqsx_key->qkd_ctx->metadata.Metadata_buffer, 0, QKD_METADATA_MAX_SIZE);
    
    // Do NOT call qkd_open() here
    // For ETSI 004:
    // - Alice will call qkd_open() during key generation (oqsx_key_gen_qkd)
    // - Bob will call qkd_open() during encapsulation (oqs_qkd_get_encaps_key)
    oqsx_key->qkd_ctx->is_connected = false;
    QKD_DEBUG("ETSI 004: Context initialized, connection deferred to key generation/encapsulation");
#endif

#ifdef ETSI_014_API
    // After URIs are set, initialize ETSI014 specific fields and check status
    qkd_status_t *status = &oqsx_key->qkd_ctx->status;
    status->source_KME_ID = NULL;
    status->target_KME_ID = NULL;
    status->master_SAE_ID = NULL;
    status->slave_SAE_ID = NULL;

// Initialize and validate status with KME
#ifdef DEBUG_QKD
    if (!qkd_get_status(oqsx_key->qkd_ctx)) {
        QKD_DEBUG("Failed to get initial QKD status");
        OPENSSL_free(oqsx_key->qkd_ctx);
        oqsx_key->qkd_ctx = NULL;
        return OQS_ERROR;
    }
#endif /* NDEBUG */
#endif

    QKD_DEBUG("QKD context initialized successfully as %s",
              is_initiator ? "initiator" : "responder");
    QKD_DEBUG("INIT: Initiator role: %d", oqsx_key->qkd_ctx->is_initiator);
    QKD_DEBUG("INIT: Connection will be established later during key operations");

    return OQS_SUCCESS;
}
