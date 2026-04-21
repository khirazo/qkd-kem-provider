# Architecture Refactoring Plan: SAE Role and Environment Variable Redesign

## Document Purpose

This document provides a comprehensive analysis of the current implementation's architectural issues and a detailed refactoring plan for future AI sessions or developers to implement improvements.

## Executive Summary

The current implementation conflates TLS roles (client/server) with QKD roles (initiator/responder) and uses misleading terminology (`master`/`slave`) that suggests permanent, topology-level roles. This violates ETSI GS QKD-014 principles and creates unnecessary constraints.

**Key Issues:**
- Environment variables use `MASTER`/`SLAVE` terminology tied to TLS roles
- Implementation assumes 1:1 topology only
- SAE concept is misunderstood as "per-connection" rather than "per-node"
- Certificate selection logic is unnecessarily complex

**Recommended Solution:**
- Rename environment variables to `MY_*` and `PEER_*` pattern
- Decouple QKD roles from TLS roles
- Simplify certificate handling (only "my" certificates needed)
- Prepare for future Key Stream support (1:N topology)

---

## 1. Background: ETSI GS QKD-014 Specification

### 1.1 Official Roles in ETSI 014

The ETSI GS QKD-014 specification defines only two official entities:

| Role | Meaning |
|------|---------|
| **SAE (Secure Application Entity)** | The logical entity that consumes keys (API client) |
| **KME (Key Management Entity)** | The entity that manages and supplies keys (API server) |

⚠️ **Important:** The terms `master`/`slave` and `initiator`/`responder` are **NOT** official ETSI terminology.

### 1.2 What initiator/responder Actually Mean

These terms represent **transient roles within a single key acquisition flow**:

- **Initiator**: The party that calls `GET_KEY()` first for a specific key
- **Responder**: The party that calls `GET_KEY_WITH_IDS()` to retrieve the same key

**Critical Points:**
- These roles are **per-key**, not per-node
- The same SAE can be initiator for one key and responder for the next
- There is no "global master" or "global slave" in QKD topology

### 1.3 Separation of Concerns

```
TLS Layer:
  TLS Client ────────▶ TLS Server
  (connection initiator)  (connection acceptor)

QKD Layer (ETSI 014):
  SAE-A ──┐
          ├──▶ KME-A
  SAE-B ──┘
  
  Both SAE-A and SAE-B are:
  - API clients to their respective KMEs
  - Peers in the QKD key exchange
  - Capable of being initiator OR responder
```

**Key Insight:** TLS client/server roles are **orthogonal** to QKD initiator/responder roles.

---

## 2. Current Implementation Analysis

### 2.1 Environment Variables (Current)

```bash
# Current naming scheme
QKD_MASTER_KME_HOSTNAME    # Interpreted as "TLS Client's KME"
QKD_SLAVE_KME_HOSTNAME     # Interpreted as "TLS Server's KME"
QKD_MASTER_SAE             # Interpreted as "TLS Client's SAE ID"
QKD_SLAVE_SAE              # Interpreted as "TLS Server's SAE ID"
QKD_MASTER_CERT_PATH       # TLS Client's certificate
QKD_SLAVE_CERT_PATH        # TLS Server's certificate
```

### 2.2 Role Assignment Logic (Current)

From `qkd-kem-provider/oqsprov/oqs_qkd_ctx.c` lines 91-100:

```c
// Set URIs based on role
if (ctx->is_initiator) {
    ctx->source_uri = ctx->master_kme;    // My KME
    ctx->dest_uri = ctx->slave_kme;       // Peer's KME (unused)
    ctx->sae_id = ctx->slave_sae;         // Peer's SAE ID
} else {
    ctx->source_uri = ctx->slave_kme;     // My KME
    ctx->dest_uri = ctx->master_kme;      // Peer's KME (unused)
    ctx->sae_id = ctx->master_sae;        // Peer's SAE ID
}
```

### 2.3 Problems with Current Design

#### Problem 1: Misleading Terminology

The terms `master`/`slave` suggest:
- ❌ Permanent, hierarchical roles
- ❌ Topology-level distinction
- ❌ One node is "superior" to another

Reality:
- ✅ Roles are transient and per-key
- ✅ Both nodes are peers
- ✅ Either can be initiator for different keys

#### Problem 2: TLS Role Coupling

Current implementation assumes:
- TLS Client = Master = Initiator (always)
- TLS Server = Slave = Responder (always)

This is **incorrect** because:
- TLS roles are about connection establishment
- QKD roles are about key acquisition
- These are independent concerns

#### Problem 3: Scalability Limitations

Current design enforces 1:1 topology:
```
TLS Client (SAE-1) ──▶ TLS Server (SAE-2)  ✅ Works

TLS Client (SAE-1) ──┬──▶ TLS Server A (SAE-2)
                     └──▶ TLS Server B (SAE-3)  ❌ Requires multiple SAEs
```

Correct design should support:
```
TLS Client (SAE-1) ──┬──▶ TLS Server A (SAE-2)  [Key Stream A]
                     └──▶ TLS Server B (SAE-3)  [Key Stream B]
```

#### Problem 4: Certificate Confusion

Current implementation requires:
- `QKD_MASTER_CERT_PATH` for TLS Client
- `QKD_SLAVE_CERT_PATH` for TLS Server

Reality:
- Each node only needs **its own** certificate
- Peer certificates are not needed for QKD API authentication

---

## 3. Proposed Refactoring

### 3.1 New Environment Variable Scheme

```bash
# Self identification
QKD_MY_KME_HOSTNAME        # The KME this node connects to
QKD_MY_SAE_ID              # This node's SAE identifier
QKD_MY_CERT_PATH           # This node's certificate
QKD_MY_KEY_PATH            # This node's private key
QKD_MY_CA_CERT_PATH        # This node's CA certificate

# Peer identification (for current key exchange)
QKD_PEER_SAE_ID            # The peer's SAE identifier

# Optional: Future Key Stream support
QKD_KEY_STREAM_ID          # Logical channel identifier (future)
```

### 3.2 Refactored Context Structure

```c
typedef struct {
    // Self information
    char *my_kme_hostname;
    char *my_sae_id;
    char *my_cert_path;
    char *my_key_path;
    char *my_ca_cert_path;
    
    // Peer information
    char *peer_sae_id;
    
    // Transient role (per-key)
    bool is_initiator;
    
    // Future: Key Stream support
    char *key_stream_id;
    
    // ... other fields
} QKD_CTX;
```

### 3.3 Refactored Initialization Logic

```c
static int qkd_init_uris(QKD_CTX *ctx) {
    // Load self information (always the same)
    const char *my_kme = getenv("QKD_MY_KME_HOSTNAME");
    const char *my_sae = getenv("QKD_MY_SAE_ID");
    const char *peer_sae = getenv("QKD_PEER_SAE_ID");
    
    if (!my_kme || !my_sae || !peer_sae) {
        return OQS_ERROR;
    }
    
    ctx->my_kme_hostname = OPENSSL_strdup(my_kme);
    ctx->my_sae_id = OPENSSL_strdup(my_sae);
    ctx->peer_sae_id = OPENSSL_strdup(peer_sae);
    
    // Role-based API selection (transient)
    if (ctx->is_initiator) {
        // GET_KEY(my_kme, peer_sae, ...)
        ctx->api_kme = ctx->my_kme_hostname;
        ctx->api_sae = ctx->peer_sae_id;
    } else {
        // GET_KEY_WITH_IDS(my_kme, peer_sae, ...)
        ctx->api_kme = ctx->my_kme_hostname;
        ctx->api_sae = ctx->peer_sae_id;
    }
    
    return OQS_SUCCESS;
}
```

### 3.4 Refactored API Calls

```c
// Initiator path
if (ctx->is_initiator) {
    // I am requesting a new key from my KME
    // The key will be shared with peer_sae
    ret = GET_KEY(ctx->my_kme_hostname, ctx->peer_sae_id, &request, &container);
}

// Responder path
else {
    // I am retrieving a key that peer_sae already requested
    // from my KME using the key_id they sent me
    ret = GET_KEY_WITH_IDS(ctx->my_kme_hostname, ctx->peer_sae_id, &key_ids, &container);
}
```

---

## 4. Migration Strategy

### 4.1 Phase 1: Add New Variables (Backward Compatible)

Support both old and new environment variables:

```c
// Try new variables first
const char *my_kme = getenv("QKD_MY_KME_HOSTNAME");
const char *my_sae = getenv("QKD_MY_SAE_ID");

// Fall back to old variables with deprecation warning
if (!my_kme) {
    my_kme = getenv("QKD_MASTER_KME_HOSTNAME");
    if (my_kme) {
        fprintf(stderr, "WARNING: QKD_MASTER_KME_HOSTNAME is deprecated. "
                        "Use QKD_MY_KME_HOSTNAME instead.\n");
    }
}
```

### 4.2 Phase 2: Update Documentation

- Mark old variables as deprecated
- Provide migration guide
- Update all examples to use new variables

### 4.3 Phase 3: Remove Old Variables

After sufficient deprecation period:
- Remove support for old variables
- Clean up fallback code
- Update all tests

---

## 5. Benefits of Refactoring

### 5.1 Conceptual Clarity

| Aspect | Before | After |
|--------|--------|-------|
| Terminology | `master`/`slave` (misleading) | `my`/`peer` (clear) |
| Role interpretation | Permanent, topology-level | Transient, per-key |
| TLS coupling | Tightly coupled | Decoupled |

### 5.2 Scalability

```
Current (1:1 only):
  Node A (SAE-1) ──▶ Node B (SAE-2)

Future (1:N capable):
  Node A (SAE-1) ──┬──▶ Node B (SAE-2) [Stream-B]
                   ├──▶ Node C (SAE-3) [Stream-C]
                   └──▶ Node D (SAE-4) [Stream-D]
```

### 5.3 Reduced Complexity

- Each node only manages its own credentials
- No need to distinguish "master" vs "slave" certificates
- Simpler configuration for users

---

## 6. Implementation Checklist

### 6.1 Code Changes

- [ ] Update `QKD_CTX` structure in `oqs_qkd_ctx.h`
- [ ] Refactor `qkd_init_uris()` in `oqs_qkd_ctx.c`
- [ ] Update API call sites in `oqs_qkd_etsi_api_wrapper.c`
- [ ] Add backward compatibility layer
- [ ] Update certificate loading logic
- [ ] Add deprecation warnings

### 6.2 Testing

- [ ] Update test environment variable setup
- [ ] Verify backward compatibility
- [ ] Test with both old and new variables
- [ ] Validate 1:1 topology still works
- [ ] Add tests for new variable scheme

### 6.3 Documentation

- [ ] Update README.md with new variables
- [ ] Add migration guide
- [ ] Update protocol documentation
- [ ] Add architecture diagrams
- [ ] Document future Key Stream support

---

## 7. Future Enhancements

### 7.1 Key Stream Support

Enable 1:N topology by adding:

```bash
QKD_KEY_STREAM_ID=stream-to-peer-B
```

This allows:
- Single SAE to communicate with multiple peers
- Logical separation of key pools
- Better alignment with ETSI 014 specification

### 7.2 Dynamic Peer Selection

Instead of static `QKD_PEER_SAE_ID`, support:
- Runtime peer selection via TLS extensions
- Multiple peer configurations
- Automatic Key Stream management

### 7.3 Certificate Management

Simplify to:
- Only `QKD_MY_CERT_PATH` required
- Automatic certificate selection
- No peer certificate configuration needed

---

## 8. References

- ETSI GS QKD 014 V1.1.1 (2019-02): Protocol and data format of REST-based key delivery API
- QURSA Project: https://github.com/qursa-uc3m
- Related Analysis: See `work/QKDとTLS統合におけるSAE_initiator_responder_トポロジー整理メモ.md`

---

## 9. Contact and Contributions

This refactoring plan is part of the QURSA project. For questions or contributions:

- Universidad Carlos III de Madrid (UC3M)
- Universidade de Vigo (UVigo)

---

**Document Version:** 1.0  
**Last Updated:** 2026-04-21  
**Status:** Proposed for Implementation