/**
 * @file LabTestScratch.cpp
 * @brief Implementacion del helper de scratch por test (ver LabTestScratch.h).
 * @author ABDSynths
 * @date 2026
 */

// Relativo a este fichero. Los tests lo incluyen como "support/LabTestScratch.h",
// que es lo que resuelve la regla de busqueda del preprocesador para un
// fichero que vive en src/tests/.
#include "LabTestScratch.h"

#include "core/LabResourcePaths.h"

#include <cctype>

namespace abdaudiolab::test
{

/**
 * @brief true si @p candidate esta dentro de (o es) la raiz del repositorio.
 *
 * Se compara con isAChildOf() sobre rutas ya normalizadas por juce::File en vez
 * de comparar cadenas: dos rutas al mismo directorio pueden escribirse con
 * barras distintas, y un guard que se deja engañar por la barra no protege de
 * nada.
 */
bool isInsideRepo (const juce::File& candidate)
{
    const auto resolution = core::resolveRepoRoot();

    if (! resolution.isResolved)
        return false;   // Sin raiz resuelta no se puede afirmar nada; no se bloquea al test.

    // isAChildOf() NO es reflexivo: un directorio no es hijo de si mismo (solo
    // es "padre" de sus contenidos). El caso de igualdad se comprueba aparte,
    // porque un scratch justo en la raiz del repo es exactamente el fallo que
    // este modulo existe para cazar.
    return candidate == resolution.root || candidate.isAChildOf (resolution.root);
}

namespace
{

/** @brief Construye la ruta del scratch de un test y garantiza que no cae en el repo. */
juce::File buildScratchPath (const juce::String& caseName)
{
    const auto safeName = sanitizeScratchName (caseName);

    if (safeName.isEmpty())
        throw std::runtime_error ("LabTestScratch: nombre de test vacio tras sanitizar");

    const auto path = scratchRoot().getChildFile (safeName);

    if (isInsideRepo (path))
    {
        throw std::runtime_error ("LabTestScratch: '" + caseName.toStdString()
                                  + "' resolveria a '" + path.getFullPathName().toStdString()
                                  + "', que esta DENTRO del repositorio. "
                                    "Las fixtures de un test se escriben en el scratch, nunca en el arbol del repo.");
    }

    return path;
}

} // namespace

juce::String sanitizeScratchName (const juce::String& name)
{
    juce::String out;

    for (auto ch : name)
    {
        if (out.length() > 96)
            break;

        const auto c = static_cast<char> (ch);

        if (std::isalnum (static_cast<unsigned char> (c)) != 0 || c == '.' || c == '_' || c == '-')
            out += ch;
        else
            out += '_';
    }

    return out;
}

juce::File scratchRoot()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("abdaudiolab-tests");
}

juce::File scratchDir (const juce::String& caseName)
{
    const auto path = buildScratchPath (caseName);

    // Se vacia SIEMPRE antes de crear. Es lo que hace que dos corridas seguidas
    // del mismo test empiecen desde el mismo estado, y que una corrida
    // interrumpida no deje un directorio a medio construir que la siguiente
    // herede como si fuera valido.
    path.deleteRecursively();

    if (! path.createDirectory())
        throw std::runtime_error ("LabTestScratch: no se pudo crear '" + path.getFullPathName().toStdString() + "'");

    return path;
}

void clearScratchDir (const juce::String& caseName)
{
    buildScratchPath (caseName).deleteRecursively();
}

bool artifactRegenerationEnabled()
{
    const auto value = juce::SystemStats::getEnvironmentVariable (kRegenerateArtifactsEnvVar, "0").trim();

    return value.equalsIgnoreCase ("1") || value.equalsIgnoreCase ("true")
            || value.equalsIgnoreCase ("yes") || value.equalsIgnoreCase ("on");
}

juce::File artifactDir (const juce::String& repoRelativeDir, const juce::String& caseName)
{
    if (! artifactRegenerationEnabled())
        return scratchDir (caseName + "_" + sanitizeScratchName (repoRelativeDir).replace ("/", "_"));

    return core::repoResource (repoRelativeDir);
}

} // namespace abdaudiolab::test
