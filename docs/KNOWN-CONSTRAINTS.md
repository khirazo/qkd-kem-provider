# Known Constraints and Design Limitations

## Document Purpose

This document records known constraints and design limitations in the current QKD-KEM Provider implementation. These constraints are inherent to the current architecture and should be considered when using or extending this provider.

---

## 1. Environment Variable Design

### 1.1 Current Variable Naming

The implementation uses `MASTER`/`SLAVE` terminology for environment variables:

```bash
# Current naming scheme
QKD_MASTER_KME_HOSTNAME    # KME endpoint for "master" role
QKD_SLAVE_KME_HOSTNAME     # KME endpoint for "slave" role
QKD_MASTER_SAE             # SAE identifier for "master" role
QKD_SLAVE_SAE              # SAE identifier for "slave" role
QKD_MASTER_CERT_PATH       # Certificate for "master" role
QKD_SLAVE_CERT_PATH        # Certificate for "slave" role
```

### 1.2 Terminology Constraints

**Constraint:** The terms `MASTER` and `SLAVE` are used to distinguish between two nodes, but:
- These terms are not part of ETSI GS QKD-014 official terminology
- They suggest permanent hierarchical roles, which contradicts ETSI principles
- They are commonly confused with TLS client/server roles

**ETSI 014 Official Terminology:**
- **SAE (Secure Application Entity)**: The logical entity that consumes keys (API client)
- **KME (Key Management Entity)**: The entity that manages and supplies keys (API server)
- **Initiator**: The party that calls `GET_KEY()` first for a specific key (transient, per-key role)
- **Responder**: The party that calls `GET_KEY_WITH_IDS()` to retrieve the same key (transient, per-key role)

**Impact:**
- Users must understand that `MASTER`/`SLAVE` are configuration labels, not ETSI roles
- The same node may need different configurations depending on its TLS role
- Documentation must clarify the distinction between TLS roles and QKD roles

---

## 2. Role Assignment Logic

### 2.1 Current Implementation

From `oqsprov/oqs_qkd_ctx.c`:

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

**Constraint:** The `is_initiator` flag determines which set of variables to use:
- TLS client typically sets `is_initiator = true` → uses `MASTER` variables
- TLS server typically sets `is_initiator = false` → uses `SLAVE` variables

**Impact:**
- TLS roles are tightly coupled with QKD variable selection
- Each node must configure both `MASTER` and `SLAVE` variables
- The same physical node needs different configurations for client vs. server roles

### 2.2 Unused Variables

**Constraint:** The `dest_uri` (peer's KME) is loaded but never used in ETSI 014 API calls.

**Reason:** Per ETSI 014 specification:
- Each SAE connects only to its own local KME
- Peer's KME is not involved in the API calls
- Key synchronization is handled by KME-to-KME communication

**Impact:**
- Configuration includes unnecessary variables (`QKD_SLAVE_KME_HOSTNAME` for master, `QKD_MASTER_KME_HOSTNAME` for slave)
- Variable naming suggests peer KME is needed, which is misleading

---

## 3. TLS and QKD Role Coupling

### 3.1 Separation of Concerns

**Constraint:** The current implementation conflates two independent concerns:

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

**Current Implementation:** Assumes TLS Client = QKD Initiator = MASTER, which limits flexibility.

### 3.2 IS_TLS_SERVER Environment Variable

**Constraint:** The `IS_TLS_SERVER` environment variable is used to determine role:

```bash
# TLS Server side
export IS_TLS_SERVER=1

# TLS Client side
# (IS_TLS_SERVER not set or set to 0)
```

**Impact:**
- Role determination happens at runtime based on environment
- Same binary requires different environment configurations
- Testing requires careful environment setup

---

## 4. Context Structure

### 4.1 Current Structure

From `oqsprov/oqs_qkd_ctx.h`:

```c
typedef struct {
    // Current fields
    char *master_kme;           // KME for "master" role
    char *slave_kme;            // KME for "slave" role
    char *master_sae;           // SAE ID for "master" role
    char *slave_sae;            // SAE ID for "slave" role
    
    // Runtime state
    char *source_uri;           // Resolved KME URI for API calls
    char *dest_uri;             // Unused (peer's KME)
    char *sae_id;               // Resolved peer SAE ID
    
    bool is_initiator;          // Role flag
    
    // ... other fields
} QKD_CTX;
```

**Constraint:** The structure stores both `MASTER` and `SLAVE` configurations, even though only one set is used per connection.

**Impact:**
- Memory overhead for unused configuration
- Initialization must load all variables regardless of role
- Structure size is larger than necessary

---

## 5. Certificate Management

### 5.1 Dual Certificate Configuration

**Constraint:** The implementation expects separate certificates for `MASTER` and `SLAVE` roles:

```bash
QKD_MASTER_CERT_PATH="/path/to/master/cert.pem"
QKD_MASTER_KEY_PATH="/path/to/master/key.pem"
QKD_MASTER_CA_CERT_PATH="/path/to/master/ca.pem"

QKD_SLAVE_CERT_PATH="/path/to/slave/cert.pem"
QKD_SLAVE_KEY_PATH="/path/to/slave/key.pem"
QKD_SLAVE_CA_CERT_PATH="/path/to/slave/ca.pem"
```

**Reality:** Certificates are used for:
- Authenticating the SAE to its local KME (mTLS)
- Not for peer-to-peer authentication (handled by TLS layer)

**Impact:**
- Each node typically uses the same certificate for both roles
- Configuration duplication without functional benefit
- Certificate rotation requires updating multiple variables

### 5.2 Certificate Selection Logic

**Constraint:** Certificate selection is based on `is_initiator` flag, which is derived from TLS role.

**Impact:**
- Same node needs different certificate configurations for client vs. server
- In practice, both sets often point to identical files
- Deployment scripts must handle redundant certificate distribution

---

## 6. Topology Constraints

### 6.1 Point-to-Point Limitation

**Constraint:** The current design assumes a 1:1 topology:

```
TLS Client (SAE-1) ←→ TLS Server (SAE-2)
```

**Limitation:** Multi-peer scenarios are not supported:

```
TLS Client (SAE-1) ──┬──▶ TLS Server A (SAE-2)
                     └──▶ TLS Server B (SAE-3)  ❌ Not supported
```

**Impact:**
- Each TLS client can only communicate with one TLS server using QKD
- Mesh topologies require multiple SAE instances
- Key Stream concept (ETSI 014 feature for 1:N) is not implemented

### 6.2 Configuration Scalability

**Constraint:** For N nodes in a mesh topology:
- Each node would need N-1 sets of peer configurations
- No mechanism for dynamic peer selection
- Environment variables are static at process startup

---

## 7. API Call Patterns

### 7.1 ETSI 014 API Usage

From `oqsprov/oqs_qkd_etsi_api_wrapper.c`:

```c
// Initiator path (master)
ret = GET_KEY(ctx->master_kme, ctx->slave_sae, &request, &container);

// Responder path (slave)
ret = GET_KEY_WITH_IDS(ctx->slave_kme, ctx->master_sae, &key_ids, &container);
```

**Constraint:** API calls use different variable combinations based on role:
- Initiator: Uses `master_kme` (my KME) and `slave_sae` (peer SAE)
- Responder: Uses `slave_kme` (my KME) and `master_sae` (peer SAE)

**Impact:**
- Variable naming does not reflect actual usage
- Both roles connect to their own KME, but use different variable names
- Code readability is reduced

### 7.2 Key ID Management

**Constraint:** Key IDs must be exchanged between initiator and responder:
- Initiator receives key ID from `GET_KEY()` response
- Key ID must be transmitted to responder via TLS extension
- Responder uses key ID in `GET_KEY_WITH_IDS()` call

**Current Implementation:** Key ID exchange is handled by the provider's TLS integration, but this creates tight coupling between QKD and TLS layers.

---

## 8. OpenSSL Provider Integration

### 8.1 Provider Initialization

**Constraint:** The QKD-KEM provider must be loaded alongside the default provider:

```bash
openssl s_server -provider default -provider qkdkemprovider ...
openssl s_client -provider default -provider qkdkemprovider ...
```

**Impact:**
- Both providers must be specified in correct order
- Configuration files must include provider directives
- Provider loading failures may not be immediately obvious

### 8.2 Algorithm Selection

**Constraint:** QKD-KEM algorithms are identified by specific group IDs:

```
qkd_frodo640aes   → 0x3000
qkd_kyber768      → 0x303C
qkd_mlkem768      → 0x3768
...
```

**Impact:**
- Wireshark and other tools may not recognize these custom group IDs
- Interoperability limited to systems with QKD-KEM provider
- Standard TLS tools show "Unknown" for QKD groups

---

## 9. Testing and Validation

### 9.1 Test Environment Requirements

**Constraint:** Full integration testing requires:
- Two separate processes (TLS client and server)
- Proper environment variable configuration for each role
- Access to KME endpoints (real or simulated)
- Certificate infrastructure

**Current Test Setup:**
```bash
# Terminal 1 (Server)
export IS_TLS_SERVER=1
export QKD_SLAVE_KME_HOSTNAME="https://kme-2.example.com"
export QKD_SLAVE_SAE="sae-2"
export QKD_MASTER_SAE="sae-1"
openssl s_server -provider default -provider qkdkemprovider ...

# Terminal 2 (Client)
export QKD_MASTER_KME_HOSTNAME="https://kme-1.example.com"
export QKD_MASTER_SAE="sae-1"
export QKD_SLAVE_SAE="sae-2"
openssl s_client -provider default -provider qkdkemprovider ...
```

**Impact:**
- Manual test setup is error-prone
- Automated testing requires complex environment management
- CI/CD pipelines need careful configuration

### 9.2 Backend-Specific Constraints

**Constraint:** Different backends have different requirements:

- **Simulated backend**: No external dependencies, works offline
- **QuKayDee backend**: Requires internet connectivity, valid account, proper certificates
- **Python client backend**: Requires Python runtime, specific module installation

**Impact:**
- Test matrix must cover multiple backend configurations
- Some tests can only run with specific backends
- Documentation must cover backend-specific setup

---

## 10. Debug and Diagnostics

### 10.1 Debug Logging

**Constraint:** Debug logging is controlled by compile-time flags:

```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug logging enabled
else()
    # Debug logging disabled (Release build)
    add_definitions(-DNDEBUG)
endif()
```

**Impact:**
- Release builds have no debug output
- Troubleshooting production issues is difficult
- Separate debug builds required for diagnostics

### 10.2 Error Reporting

**Constraint:** Error messages may not clearly indicate:
- Which environment variable is missing or misconfigured
- Whether the issue is with `MASTER` or `SLAVE` configuration
- The relationship between TLS role and variable selection

**Impact:**
- Users may struggle to diagnose configuration issues
- Support requests require detailed environment inspection
- Error messages need improvement for better usability

---

## 11. Dependency on qkd-etsi-api-c-wrapper

### 11.1 API Wrapper Constraints

**Constraint:** This provider depends on the qkd-etsi-api-c-wrapper library, which has its own constraints:

- Environment variable naming (`MASTER`/`SLAVE`)
- Certificate configuration requirements
- Backend-specific limitations

**Impact:**
- Constraints from the wrapper library propagate to this provider
- Changes to wrapper API require provider updates
- Both libraries must be kept in sync

### 11.2 Version Compatibility

**Constraint:** The provider is tested with specific versions of:
- OpenSSL (3.4.0)
- liboqs (0.12.0)
- qkd-etsi-api-c-wrapper (latest)

**Impact:**
- Version mismatches may cause runtime issues
- Deployment must ensure compatible versions
- Testing matrix includes version combinations

---

## 12. Future Considerations

### 12.1 Potential Improvements

While this document focuses on current constraints, potential areas for future improvement include:

1. **Simplified variable naming** (e.g., `MY_*` and `PEER_*` instead of `MASTER`/`SLAVE`)
2. **Role-independent configuration** (single set of variables per node)
3. **Runtime debug logging** (controlled by environment variable, not compile-time flag)
4. **Key Stream support** for 1:N topologies
5. **Dynamic peer selection** without environment variable changes
6. **Improved error diagnostics** with context-aware messages

### 12.2 Compatibility Requirements

Any future improvements must:
- Maintain backward compatibility with existing deployments
- Coordinate with qkd-etsi-api-c-wrapper changes
- Provide clear migration paths
- Include comprehensive testing across all backends
- Update documentation and examples

---

## 13. References

- **ETSI GS QKD 014 V1.1.1 (2019-02)**: Protocol and data format of REST-based key delivery API
- **OpenSSL 3.x Provider API**: https://www.openssl.org/docs/man3.0/man7/provider.html
- **liboqs**: https://github.com/open-quantum-safe/liboqs
- **qkd-etsi-api-c-wrapper**: https://github.com/qursa-uc3m/qkd-etsi-api-c-wrapper
- **Project Repository**: https://github.com/qursa-uc3m/qkd-kem-provider

