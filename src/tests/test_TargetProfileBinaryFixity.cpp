#include <catch2/catch_test_macros.hpp>
#include "profiling/TargetProfileService.h"

using namespace abdaudiolab::profiling;

TEST_CASE("HITO-10C: TargetProfile Binary Fixity Audit", "[target_profile][fixity]")
{
    TargetProfileService service;
    const std::string kCanonicalSha256 = "e8b3b00a53bb0aa1ef082b0c1b5cb66bdf1af5eddf787b8f47397df6d2411a40";
    const std::string kAlteredSha256   = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";

    TargetProfile profile;
    profile.targetProfileId = "test-target-fixity";
    profile.identity.canonicalTargetId = "test-canonical-id";
    profile.identity.expectedBinarySha256 = kCanonicalSha256;

    SECTION("1. Coincidencia exacta de hash binario (PASS para cualquier politica)")
    {
        profile.identity.binaryIdentityPolicy = "warn-on-mismatch";
        auto resWarn = service.auditBinaryFixity(profile, kCanonicalSha256);
        CHECK(resWarn.passed);
        CHECK_FALSE(resWarn.isWarning);
        CHECK(resWarn.diagnostics.empty());

        profile.identity.binaryIdentityPolicy = "require-audit-on-change";
        auto resReq = service.auditBinaryFixity(profile, kCanonicalSha256);
        CHECK(resReq.passed);
        CHECK_FALSE(resReq.isWarning);
        CHECK(resReq.diagnostics.empty());

        profile.identity.binaryIdentityPolicy = "strict-bit-exact";
        auto resStrict = service.auditBinaryFixity(profile, kCanonicalSha256);
        CHECK(resStrict.passed);
        CHECK_FALSE(resStrict.isWarning);
        CHECK(resStrict.diagnostics.empty());
    }

    SECTION("2. Politica warn-on-mismatch: divergencia emite Warning no bloqueante")
    {
        profile.identity.binaryIdentityPolicy = "warn-on-mismatch";
        auto res = service.auditBinaryFixity(profile, kAlteredSha256);

        CHECK(res.passed); // No bloquea la ejecución
        CHECK(res.isWarning);
        REQUIRE(res.diagnostics.size() == 1);
        CHECK(res.diagnostics[0].severity == DiagnosticSeverity::Warning);
        CHECK(res.diagnostics[0].code == "WARN_TARGET_PROFILE_BINARY_MISMATCH");
    }

    SECTION("3. Politica require-audit-on-change: divergencia emite Error bloqueante")
    {
        profile.identity.binaryIdentityPolicy = "require-audit-on-change";
        auto res = service.auditBinaryFixity(profile, kAlteredSha256);

        CHECK_FALSE(res.passed); // Bloquea la ejecución
        CHECK(res.hasErrors());
        REQUIRE(res.diagnostics.size() == 1);
        CHECK(res.diagnostics[0].severity == DiagnosticSeverity::Error);
        CHECK(res.diagnostics[0].code == "ERR_TARGET_PROFILE_BINARY_MISMATCH");
    }

    SECTION("4. Politica strict-bit-exact: divergencia emite Error estricto")
    {
        profile.identity.binaryIdentityPolicy = "strict-bit-exact";
        auto res = service.auditBinaryFixity(profile, kAlteredSha256);

        CHECK_FALSE(res.passed); // Rechazo terminante
        CHECK(res.hasErrors());
        REQUIRE(res.diagnostics.size() == 1);
        CHECK(res.diagnostics[0].severity == DiagnosticSeverity::Error);
        CHECK(res.diagnostics[0].code == "ERR_TARGET_PROFILE_BINARY_STRICT_MISMATCH");
    }

    SECTION("5. Politica not-applicable: divergencia ignorada limpiamente")
    {
        profile.identity.binaryIdentityPolicy = "not-applicable";
        auto res = service.auditBinaryFixity(profile, kAlteredSha256);

        CHECK(res.passed);
        CHECK_FALSE(res.isWarning);
        CHECK(res.diagnostics.empty());
    }

    SECTION("6. Target observado sin hash proporcionado bajo politicas activas")
    {
        profile.identity.binaryIdentityPolicy = "warn-on-mismatch";
        auto resWarn = service.auditBinaryFixity(profile, "");
        CHECK(resWarn.passed);
        CHECK(resWarn.isWarning);
        REQUIRE(resWarn.diagnostics.size() == 1);
        CHECK(resWarn.diagnostics[0].code == "WARN_TARGET_PROFILE_BINARY_MISSING");

        profile.identity.binaryIdentityPolicy = "require-audit-on-change";
        auto resReq = service.auditBinaryFixity(profile, "");
        CHECK_FALSE(resReq.passed);
        REQUIRE(resReq.diagnostics.size() == 1);
        CHECK(resReq.diagnostics[0].code == "ERR_TARGET_PROFILE_BINARY_MISSING");
    }
}
