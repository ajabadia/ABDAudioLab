/**
 * @file LabTestScratch.h
 * @brief Directorio temporal exclusivo por test, y destino de los artefactos
 *        versionados que la suite puede regenerar de forma opt-in.
 *
 * QUE PROBLEMA RESUELVE
 * ---------------------
 * POST-5D.5 elimino la dependencia del directorio de trabajo, pero dejo otra
 * fuente de no-hermeticidad igual de silenciosa: los tests ESCRIBIAN dentro del
 * arbol del repositorio. Tres de ellos regeneraban artefactos canónicos
 * versionados (docs/qa/runs/*.json, fixtures/evaluations/*.json,
 * assets/presets/*.json), lo que hacia que una corrida de la suite dejara
 * git status sucio. Eso no es cosmetico: un gate que dice "el arbol esta
 * limpio" se vuelve imposible de evaluar cuando el propio test ensucia el
 * arbol, y un artefacto regenerado sin querer se confunde con un cambio de
 * codigo en el siguiente commit.
 *
 * Este modulo centraliza las dos unicas salidas legitimas:
 *
 *   1. scratchDir()     — todo lo que la suite fabrica para si misma (wavs
 *                         sinteticos, json de prueba, paquetes de sesion)
 *                         vive en %TEMP%/abdaudiolab-tests/<nombre-del-test>,
 *                         se vacia al entrar y se borra al salir.
 *
 *   2. artifactDir()    — el destino de los artefactos CANONICOS DEL REPO.
 *                         Por defecto devuelve el scratch: correr la suite no
 *                         toca el repositorio. Solo si se exporta
 *                         ABD_REGENERATE_ARTIFACTS=1 devuelve el directorio
 *                         real del repo, porque entonces quien lo pide ha
 *                         declarado conscientemente que va a regenerarlos.
 *
 * POR QUE ES UN HELPER Y NO UNA CONVENCION
 * ---------------------------------------
 * Cuarenta ficheros de testaban haciendo
 * getSpecialLocation(tempDirectory).getChildFile("nombre_fijo"), lo que deja
 * tres fallos que un guard no puede cazar:
 *
 *   - Nombre fijo compartido: dos tests con el mismo nombre de fichero se
 *     pisan, y una corrida deja restos que la siguiente hereda.
 *   - Sin limpieza: los restos se acumulan en %TEMP% indefinidamente.
 *   - Sin negacion: nada impide que un nombre mal elegido acabe dentro del
 *     repositorio. Aqui no hay "mal elegido": scratchDir() LANZA si el
 *     directorio cae dentro de la raiz del repositorio, y artifactDir() es la
 *     unica via por la que un test puede escribir en el repo.
 *
 * COMO SE USA
 * -----------
 *     TEST_CASE("...", "...")
 *     {
 *         auto scratch = abdaudiolab::test::ScratchDir ("measurement_persistence");
 *         const auto wav = scratch.path().getChildFile ("dummy.wav");
 *         ...
 *     }   // <- el directorio desaparece al salir del test
 *
 * Los tests que necesitan conservarlo mas alla del caso (compararlo con otro
 * test, depurar a mano) usan abdaudiolab::test::scratchDir() directamente.
 *
 * @author ABDSynths
 * @date 2026
 */

#pragma once

#include <juce_core/juce_core.h>

#include "core/LabResourcePaths.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace abdaudiolab::test
{

/** Variable de entorno que habilita la escritura de artefactos en el repo. */
inline constexpr const char* kRegenerateArtifactsEnvVar = "ABD_REGENERATE_ARTIFACTS";

/**
 * @brief Normaliza un nombre de test a algo seguro como nombre de directorio.
 *
 * Los nombres de TEST_CASE llevan acentos, comas, comillas y corchetes
 * (Catch2 los usa para identificar el caso). Todo eso es legal en un nombre de
 * fichero en Windows salvo unos pocos caracteres, pero depender de la lista de
 * prohibidos del sistema de ficheros es una fuente de fallos por entorno:
 * aqui se sustituye cualquier cosa que no sea [A-Za-z0-9._-] por '_'.
 */
[[nodiscard]] juce::String sanitizeScratchName (const juce::String& name);

/**
 * @brief Directorio temporal exclusivo del test identificado por @p caseName.
 *
 * Se vacia (recursivamente) en cada llamada, de modo que dos corridas seguidas
 * del mismo test empiezan siempre desde el mismo estado, y con una corrida
 * interrumpida no se hereda un directorio a medio construir.
 *
 * @throws std::runtime_error si el directorio resultante cae dentro de la raiz
 *         del repositorio, o si no se puede crear. Es una excepcion deliberada
 *         y no un aviso: significa que alguien ha metido una ruta del repo en
 *         un nombre de test, y el fallo tiene que ser ruidoso.
 */
[[nodiscard]] juce::File scratchDir (const juce::String& caseName);

/**
 * @brief true si @p candidate cae dentro de (o es) la raiz del repositorio.
 *
 * Es la invariante que hace que el helper sirva de algo, asi que se expone
 * publica y no enterrada en un .cpp anónimo: un guard puede comprobarla.
 *
 * Devuelve false si la raiz del repositorio no se pudo resolver. Es
 * deliberado: sin raiz conocida no se puede afirmar nada, y bloquear al test
 * por una Incertidumbre del entorno seria un fallo mas ruidoso que el problema
 * que se quiere cazar.
 */
[[nodiscard]] bool isInsideRepo (const juce::File& candidate);

/** @brief Raiz de todos los scratch dirs: %TEMP%/abdaudiolab-tests. */
[[nodiscard]] juce::File scratchRoot();

/**
 * @brief true si el proceso fue lanzado para REGENERAR artefactos del repo.
 *
 * Se lee del entorno en cada llamada, sin cachear, igual que resolveRepoRoot():
 * un test que manipula el entorno tiene que poder observar el efecto de
 * inmediato.
 */
[[nodiscard]] bool artifactRegenerationEnabled();

/**
 * @brief Destino de escritura de un artefacto canonico del repositorio.
 *
 * @param repoRelativeDir  Directorio del repo que aloja el artefacto, p. ej.
 *                         "docs/qa/runs". Se pasa como ruta RELATIVA para que
 *                         este helper sea el unico que conoce la raiz del
 *                         repo; ningun test debe resolverla por su cuenta.
 * @param caseName         Nombre del test, usado para el scratch por defecto.
 *
 * @return El directorio real del repo si artifactRegenerationEnabled(), y un
 *         scratch vacio y temporal en caso contrario.
 *
 * NEVERA: esta es la unica funcion del proyecto que devuelve una ruta del
 * repositorio con fines de ESCRITURA. El guard [hygiene] prohibe en
 * src/tests/*.cpp cualquier otra via, asi que "ningun test escribe en ficheros
 * versionados" deja de ser una convencion y pasa a ser una invariante
 * comprobada.
 */
[[nodiscard]] juce::File artifactDir (const juce::String& repoRelativeDir,
                                       const juce::String& caseName);

/**
 * @brief Borra el scratch de un test. Idempotente.
 */
void clearScratchDir (const juce::String& caseName);

/**
 * @brief RAII: crea el scratch de un test y lo borra al destruirse.
 *
 * Es la forma recomendada. Que la limpieza sea automatico es lo que evita el
 * segundo problema del inventario (acumulacion en %TEMP%), y que el nombre
 * viaje con el objeto es lo que evita el primero (colisiones entre tests con
 * el mismo nombre de fichero).
 */
class ScratchDir final
{
public:
    explicit ScratchDir (const juce::String& caseName)
        : caseName_ (caseName),
          path_ (scratchDir (caseName))
    {
    }

    ~ScratchDir()
    {
        try
        {
            clearScratchDir (caseName_);
        }
        catch (...)
        {
            // Un destructor no lanza. Si la limpieza falla, el directorio sobra
            // en %TEMP%, que es el menor de los dos problemas posibles; perder
            // la excepcion aqui convertiria un fallo de permisos en un
            // std::terminate durante el desenrollado de la pila.
        }
    }

    ScratchDir (const ScratchDir&) = delete;
    ScratchDir& operator= (const ScratchDir&) = delete;

    /** @brief Ruta del directorio. El directorio existe al construir el objeto. */
    [[nodiscard]] const juce::File& path() const noexcept { return path_; }

    /** @brief Azucar para path().getChildFile (name). */
    [[nodiscard]] juce::File child (const juce::String& name) const
    {
        return path_.getChildFile (name);
    }

    /** @brief Crea (si hace falta) un subdirectorio y devuelve su ruta. */
    [[nodiscard]] juce::File childDir (const juce::String& name) const
    {
        const auto childPath = path_.getChildFile (name);
        childPath.createDirectory();
        return childPath;
    }

private:
    juce::String caseName_;
    juce::File path_;
};

} // namespace abdaudiolab::test