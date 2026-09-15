#include <catch2/catch_test_macros.hpp>
#include "export/ModelExportNaming.h"
#include <juce_core/juce_core.h>

using namespace abdaudiolab::exporting;

TEST_CASE("ModelExportNaming: Sanitizacion estricta y nombres reservados de Windows", "[export][naming]")
{
    SECTION("Sanitiza caracteres invalidos en SO (espacios, barras, caracteres especiales)")
    {
        std::string input = "ReferenceSynth / VST3: Edition (Special*?<>|\")";
        std::string safe = ModelExportNaming::sanitizeComponent(input, "Default", 32);

        CHECK(safe.find('/') == std::string::npos);
        CHECK(safe.find('\\') == std::string::npos);
        CHECK(safe.find(':') == std::string::npos);
        CHECK(safe.find('*') == std::string::npos);
        CHECK(safe.find('?') == std::string::npos);
        CHECK(safe.find('<') == std::string::npos);
        CHECK(safe.find('>') == std::string::npos);
        CHECK(safe.find('|') == std::string::npos);
        CHECK(safe.find('"') == std::string::npos);
        CHECK(safe.find(' ') == std::string::npos);
        CHECK(safe.find("__") == std::string::npos); // Sin guiones bajos dobles
    }

    SECTION("Previene nombres de dispositivos reservados en Windows (CON, PRN, AUX, NUL, COM1...)")
    {
        CHECK(ModelExportNaming::sanitizeComponent("CON") == "_CON");
        CHECK(ModelExportNaming::sanitizeComponent("con") == "_con");
        CHECK(ModelExportNaming::sanitizeComponent("AUX") == "_AUX");
        CHECK(ModelExportNaming::sanitizeComponent("NUL") == "_NUL");
        CHECK(ModelExportNaming::sanitizeComponent("COM1") == "_COM1");
        CHECK(ModelExportNaming::sanitizeComponent("LPT3") == "_LPT3");
    }

    SECTION("Cadena vacia o compuesta solo de caracteres no validos retorna fallback")
    {
        CHECK(ModelExportNaming::sanitizeComponent("", "FallbackTarget") == "FallbackTarget");
        CHECK(ModelExportNaming::sanitizeComponent("   ///:::***", "FallbackModel") == "FallbackModel");
    }

    SECTION("Limita longitud maxima respetando integridad de caracteres")
    {
        std::string longName = "A_Very_Long_Synthesizer_Name_That_Exceeds_Normal_Length_Limits_For_Paths";
        std::string safe = ModelExportNaming::sanitizeComponent(longName, "Fallback", 20);
        CHECK(safe.size() <= 20);
        CHECK(safe.back() != '_');
    }
}

TEST_CASE("ModelExportNaming: Formato UTC Timestamp y Hash Prefix", "[export][naming]")
{
    SECTION("Timestamp UTC ISO-8601 compacto con T y Z")
    {
        // 2026-09-15 09:53:20 UTC (isLocalTime = false)
        juce::Time fixedTime(2026, 8, 15, 9, 53, 20, 0, false);
        std::string formatted = ModelExportNaming::formatUtcTimestamp(fixedTime);

        CHECK(formatted == "20260915T095320Z");
        CHECK(formatted.find('T') != std::string::npos);
        CHECK(formatted.back() == 'Z');
    }

    SECTION("Hash prefix: primeros 8 caracteres forzando minusculas")
    {
        std::string fullHash = "8C12CE9037F9F1F4AB005A789B577E0B22325CB995B14EC415968930ABC98C6C";
        std::string prefix = ModelExportNaming::extractHashPrefix(fullHash, 8);

        CHECK(prefix == "8c12ce90");
        CHECK(prefix.size() == 8);

        // Caso hash corto o con caracteres no validos
        CHECK(ModelExportNaming::extractHashPrefix("AB12", 8) == "ab120000");
        CHECK(ModelExportNaming::extractHashPrefix("", 8) == "00000000");
    }
}

TEST_CASE("ModelExportNaming: Construccion del nombre canonico completo", "[export][naming]")
{
    juce::Time fixedTime(2026, 8, 15, 9, 53, 20, 0, false);
    std::string fullHash = "8c12ce9037f9f1f4ab005a789b577e0b22325cb995b14ec415968930abc98c6c";

    std::string fileName = ModelExportNaming::buildFileName(
        "ReferenceSynth VST3 (Worker Aislado IPC)",
        "LUT_SIMD_2D",
        fixedTime,
        fullHash
    );

    // Formato esperado: <Target>_<Model>_<Timestamp>_<HashPrefix>.h
    CHECK(fileName == "ReferenceSynth_VST3_Worker_Aisla_LUT_SIMD_2D_20260915T095320Z_8c12ce90.h");
}

TEST_CASE("ModelExportNaming: Resolucion unica contra sobrescritura de archivos", "[export][naming]")
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ABDAudioLab_ExportNaming_Tests_" + juce::String(juce::Random::getSystemRandom().nextInt()));

    tempDir.createDirectory();

    std::string baseFile = "TestSynth_LUT_SIMD_2D_20260915T095320Z_8c12ce90.h";

    // 1. Cuando no existe, devuelve la ruta base
    juce::File file1 = ModelExportNaming::resolveUniqueExportFile(tempDir, baseFile);
    CHECK(file1.getFileName().toStdString() == baseFile);

    // Crear el archivo para simular exportacion previa
    file1.replaceWithText("// Contenido 1");
    CHECK(file1.existsAsFile());

    // 2. Segunda exportacion con el mismo nombre base no sobrescribe: genera _1.h
    juce::File file2 = ModelExportNaming::resolveUniqueExportFile(tempDir, baseFile);
    CHECK(file2.getFileName().toStdString() == "TestSynth_LUT_SIMD_2D_20260915T095320Z_8c12ce90_1.h");
    CHECK_FALSE(file2.existsAsFile());

    file2.replaceWithText("// Contenido 2");

    // 3. Tercera exportacion genera _2.h
    juce::File file3 = ModelExportNaming::resolveUniqueExportFile(tempDir, baseFile);
    CHECK(file3.getFileName().toStdString() == "TestSynth_LUT_SIMD_2D_20260915T095320Z_8c12ce90_2.h");

    // 4. El archivo original permanece inalterado
    CHECK(file1.loadFileAsString().toStdString() == "// Contenido 1");

    // Limpieza
    tempDir.deleteRecursively();
}
