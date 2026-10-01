#include "HardwareContractRegistry.h"
#include "profiling/TargetProfileService.h"
#include "profiling/TargetProfileLegacyAdapter.h"
#include <fstream>
#include <algorithm>

namespace abdaudiolab::core
{

// -----------------------------------------------------------------------------
// LA LECTURA DE `setupActions`, Y POR QUE NO ESTA DENTRO DE ESTA CLASE.
//
// Hay dos puertas al catalogo de contratos y las dos necesitan leer esta parte
// del formato: el registro local, aqui, y `SharedHardwareContractAdapter`, que
// entra por el registro compartido de ABDSharedCode. La lectura estaba escrita
// dos veces, byte a byte, como lambda local en cada una.
//
// Dos copias de una regla de parseo es el modo de fallo mas barato que existe:
// una acepta un alias que la otra ignora y el contrato se comporta distinto
// segun por donde entre, sin que nada se ponga rojo. Se unifica aqui porque las
// dosTIENEN que decir lo mismo, no porque haya dado ya un problema.
//
// Y no es una API del registro: es la lectura del formato, asi que vive fuera
// de la clase y al lado de los tipos que devuelve.
// -----------------------------------------------------------------------------
void parseSetupActions (const nlohmann::json& arrJson,
                        std::vector<HardwareSetupAction>& actions)
{
    if (! arrJson.is_array())
        return;

    for (const auto& aJson : arrJson)
    {
        if (! aJson.is_object())
            continue;

        HardwareSetupAction act;
        act.description = aJson.value ("description", "");

        const auto methodStr = aJson.value ("method", "MIDI_CC");

        if (methodStr == "NRPN")              act.method = HardwareMethod::NRPN;
        else if (methodStr == "SYSEX_RAW")    act.method = HardwareMethod::SYSEX_RAW;
        else if (methodStr == "MANUAL_PROMPT") act.method = HardwareMethod::MANUAL_PROMPT;
        else                                  act.method = HardwareMethod::MIDI_CC;

        act.channel = aJson.value ("channel", 1);
        act.controlNumber = aJson.value ("controlNumber",
                                         aJson.value ("cc", aJson.value ("nrpn", -1)));
        act.normalizedValue = aJson.value ("normalizedValue",
                                           aJson.value ("value", 0.0f));
        act.sysexHexPayload = aJson.value ("sysexHexPayload",
                                           aJson.value ("sysexHex", ""));
        act.settlingDelayMs = aJson.value ("settlingDelayMs", 50);
        actions.push_back (act);
    }
}

namespace
{

struct CertifiedParityPair
{
    std::string canonicalId;
    std::string legacyId;
};

const std::vector<CertifiedParityPair>& getCertifiedParityPairs()
{
    static const std::vector<CertifiedParityPair> pairs = {
        { "hw-behringer-pro800-canonical", "behringer_pro800" },
        { "hw-yamaha-dx7-canonical", "yamaha_dx7" },
        { "hw-boss-ds1-canonical", "boss_ds1_distortion" }
    };
    return pairs;
}

const std::unordered_set<std::string>& getCertifiedParityLegacyIds()
{
    static const std::unordered_set<std::string> certified = {
        "behringer_pro800",
        "yamaha_dx7",
        "boss_ds1_distortion"
    };
    return certified;
}

} // namespace

bool HardwareContractRegistry::loadProfileResilient(const juce::File& jsonFile, HardwareContract& outContract, juce::String& outWarning)
{
    outWarning.clear();

    if (!jsonFile.existsAsFile())
    {
        outWarning = "El archivo '" + jsonFile.getFileName() + "' no existe o no es accesible.";
        return false;
    }

    try
    {
        std::ifstream ifs(jsonFile.getFullPathName().toStdString());
        if (!ifs.is_open())
        {
            outWarning = "No se pudo abrir el archivo '" + jsonFile.getFileName() + "' para lectura.";
            return false;
        }

        nlohmann::json j;
        ifs >> j;

        if (!j.is_object())
        {
            outWarning = "El archivo '" + jsonFile.getFileName() + "' no contiene un objeto JSON raíz válido.";
            return false;
        }

        // Validación Estructural Estricta
        if (!j.contains("id") || !j["id"].is_string() || j["id"].get<std::string>().empty())
        {
            outWarning = "Perfil '" + jsonFile.getFileName() + "' omitido: falta la clave obligatoria 'id' o no es una cadena válida.";
            return false;
        }

        if (!j.contains("displayName") || !j["displayName"].is_string() || j["displayName"].get<std::string>().empty())
        {
            outWarning = "Perfil '" + jsonFile.getFileName() + "' omitido: falta la clave obligatoria 'displayName' o está vacía.";
            return false;
        }

        HardwareContract c;
        c.schemaVersion = j.value("schemaVersion", std::string("2.0"));
        c.id = j["id"].get<std::string>();
        if (j.contains("aliases") && j["aliases"].is_array())
        {
            for (const auto& a : j["aliases"])
            {
                if (a.is_string())
                    c.aliases.push_back(a.get<std::string>());
            }
        }
        c.displayName = j["displayName"].get<std::string>();
        c.description = j.value("description", std::string(""));
        c.deviceType = j.value("deviceType", j.value("category", std::string("MANUAL_EURORACK")));
        // El estado editorial viaja con el contrato. Se copia para que un
        // contrato YA retenido sea visible como tal en cualquier consumidor que
        // lo reciba por otra via —el adaptador, un snapshot—, y no solo como una
        // ausencia en esta lista.
        c.status = j.value("status", std::string());
        c.statusReason = j.value("statusReason", std::string());
        c.brand = j.value("brand", std::string(""));
        c.brandLogo = j.value("brandLogo", std::string(""));
        c.modelImage = j.value("modelImage", std::string(""));
        c.theme = j.value("theme", std::string("audiolab-light"));

        // Parse MIDI Identification
        if (j.contains("midiIdentification") && j["midiIdentification"].is_object())
        {
            const auto& midiObj = j["midiIdentification"];
            c.manufacturer = midiObj.value("manufacturer", c.brand);
            if (c.manufacturer.empty()) c.manufacturer = c.brand;
            c.model = midiObj.value("model", std::string(""));
            c.modelIdHex = midiObj.value("modelIdHex", std::string(""));
            c.autoDetectSysEx = midiObj.value("autoDetectSysEx", std::string(""));

            c.midiIdentity.manufacturer = c.manufacturer;
            c.midiIdentity.manufacturerIdHex = midiObj.value("manufacturerIdHex", std::string(""));
            c.midiIdentity.model = c.model;
            c.midiIdentity.modelIdHex = c.modelIdHex;
            c.midiIdentity.familyIdHex = midiObj.value("familyIdHex", std::string(""));
            c.midiIdentity.sysexHeaderHex = midiObj.value("sysexHeaderHex", std::string(""));

            if (midiObj.contains("portNameMatches") && midiObj["portNameMatches"].is_array())
            {
                for (const auto& item : midiObj["portNameMatches"])
                {
                    if (item.is_string())
                        c.midiIdentity.portNameMatches.push_back(item.get<std::string>());
                }
            }
        }

        // Helper lambdas for actions and measurement recipes

        auto parseRecipe = [&](const nlohmann::json& rJson, MeasurementPresetRecipe& recipe) {
            if (!rJson.is_object()) return;
            recipe.recipeType = rJson.value("recipeType", std::string("DIRECT_AUDIO_IN"));
            recipe.description = rJson.value("description", std::string(""));
            recipe.postSettlingDelayMs = rJson.value("postSettlingDelayMs", 100);

            if (rJson.contains("setupActions"))
                parseSetupActions (rJson["setupActions"], recipe.setupActions);

            if (rJson.contains("excitationNotes") && rJson["excitationNotes"].is_array())
            {
                for (const auto& nJson : rJson["excitationNotes"])
                {
                    if (!nJson.is_object()) continue;
                    NoteSequenceEvent ev;
                    ev.noteNumber = nJson.value("noteNumber", nJson.value("note", 60));
                    ev.velocity = nJson.value("velocity", 100);
                    ev.startDelayMs = nJson.value("startDelayMs", 0);
                    ev.durationMs = nJson.value("durationMs", 1000);
                    ev.isLegato = nJson.value("isLegato", false);
                    recipe.excitationNotes.push_back(ev);
                }
            }
        };

        // Parse Lifecycle Contract (Data-Driven Hardware Setup / Teardown)
        if (j.contains("lifecycle") && j["lifecycle"].is_object())
        {
            const auto& lc = j["lifecycle"];
            if (lc.contains("preCalibrationSetup"))
                parseSetupActions (lc["preCalibrationSetup"], c.lifecycle.preCalibrationSetup);
            if (lc.contains("preSessionSetup"))
                parseSetupActions (lc["preSessionSetup"], c.lifecycle.preSessionSetup);
            if (lc.contains("postSessionTeardown"))
                parseSetupActions (lc["postSessionTeardown"], c.lifecycle.postSessionTeardown);
        }

        // Parse Functions (Schema v2)
        if (j.contains("functions") && j["functions"].is_array())
        {
            for (const auto& fJson : j["functions"])
            {
                if (!fJson.is_object()) continue;
                HardwareFunction f;
                f.id = fJson.value("id", std::string("main"));
                f.name = fJson.value("name", std::string("Main Function"));
                f.blockType = fJson.value("blockType", std::string("SpectrumFilter"));
                f.suggestedStimulus = fJson.value("suggestedStimulus", std::string("LOG_SINE_SWEEP"));
                f.captureMode = fJson.value("captureMode", std::string("FIXED_TIME"));
                f.defaultBurstDurationSec = fJson.value("defaultBurstDurationSec", 1.0f);
                f.maxTimeoutSec = fJson.value("maxTimeoutSec", 60.0f);
                f.silenceThresholdDb = fJson.value("silenceThresholdDb", -60.0f);

                if (fJson.contains("routingGuide") && fJson["routingGuide"].is_object())
                {
                    const auto& rg = fJson["routingGuide"];
                    f.routingGuide.stimulusOutput = rg.value("stimulusOutput", std::string(""));
                    f.routingGuide.responseInput = rg.value("responseInput", std::string(""));
                    f.routingGuide.notes = rg.value("notes", std::string(""));
                }

                if (fJson.contains("measurementRecipe") && fJson["measurementRecipe"].is_object())
                {
                    parseRecipe(fJson["measurementRecipe"], f.measurementRecipe);
                }

                if (fJson.contains("controls") && fJson["controls"].is_array())
                {
                    for (const auto& cJson : fJson["controls"])
                    {
                        if (!cJson.is_object()) continue;
                        HardwareControl ctrl;
                        ctrl.index = cJson.value("index", static_cast<int>(f.controls.size() + 1));
                        ctrl.name = cJson.value("name", std::string("Control"));
                        ctrl.type = cJson.value("type", std::string("Knob"));
                        ctrl.controlMethod = cJson.value("controlMethod", std::string("MANUAL"));
                        ctrl.ccNumber = cJson.value("cc", cJson.value("midiCC", -1));
                        ctrl.nrpnNumber = cJson.value("nrpn", -1);
                        ctrl.sysexAddress = cJson.value("sysexAddress", std::string(""));
                        // El nombre unico del rango es `minVal`/`maxVal`/`defaultVal`
                        // (ABDSharedAssets/contracts/hardware/hardware_profile.schema.json
                        // lo declara, y 68 controles de este catalogo lo usan). El corto
                        // se tolera como segundo nombre a proposito: aqui no se valida
                        // nada, y un contrato que venga con el nombre viejo caeria al
                        // 0.0/1.0/0.5 de abajo SIN AVISAR, que es un rango plausible y
                        // por tanto invisible. Con el nombre viejo como fallback el
                        // valor equivocado solo puede venir de un contrato que no
                        // declare ninguno de los dos, y ese si es un defecto de datos.
                        ctrl.minVal = cJson.value("minVal", cJson.value("min", 0.0f));
                        ctrl.maxVal = cJson.value("maxVal", cJson.value("max", 1.0f));
                        ctrl.defaultVal = cJson.value("defaultVal", cJson.value("default", 0.5f));
                        ctrl.unit = cJson.value("unit", std::string(""));

                        if (cJson.contains("options") && cJson["options"].is_array())
                        {
                            for (const auto& opt : cJson["options"])
                            {
                                if (opt.is_string())
                                    ctrl.options.push_back(opt.get<std::string>());
                            }
                        }
                        f.controls.push_back(ctrl);
                    }
                }
                c.functions.push_back(f);
            }
        }
        // Backward Compatibility: Schema v1 "parameters" -> Single Default Function
        else if (j.contains("parameters") && j["parameters"].is_array())
        {
            HardwareFunction f;
            f.id = "main_function";
            f.name = c.displayName;
            f.blockType = "SpectrumFilter";
            f.suggestedStimulus = "LOG_SINE_SWEEP";
            f.routingGuide.stimulusOutput = "Audio Out 1 (L) -> Hardware Input";
            f.routingGuide.responseInput = "Hardware Output -> Audio In 1 (L)";
            f.routingGuide.notes = "Standard audio loopback routing.";

            for (const auto& pJson : j["parameters"])
            {
                if (!pJson.is_object()) continue;
                HardwareControl ctrl;
                ctrl.index = pJson.value("index", static_cast<int>(f.controls.size() + 1));
                ctrl.name = pJson.value("name", std::string("Param"));
                ctrl.type = pJson.value("type", std::string("Knob"));
                ctrl.defaultVal = pJson.value("default", 0.5f);
                f.controls.push_back(ctrl);
            }
            c.functions.push_back(f);
        }

        outContract = std::move(c);
        return true;
    }
    catch (const std::exception& e)
    {
        outWarning = "Error parseando perfil de hardware '" + jsonFile.getFileName() + "': " + juce::String(e.what());
        return false;
    }
}

bool HardwareContractRegistry::loadContractsFromDirectory(const juce::File& contractsDir)
{
    warnings.clear();

    if (!contractsDir.isDirectory())
    {
        lastErrorMessage = "Contracts directory does not exist: " + contractsDir.getFullPathName().toStdString();
        juce::Logger::writeToLog("[HardwareContractRegistry] " + juce::String(lastErrorMessage));
        return false;
    }

    auto files = contractsDir.findChildFiles(juce::File::findFiles, false, "*.json");
    if (files.isEmpty())
    {
        lastErrorMessage = "No JSON contracts found in directory: " + contractsDir.getFullPathName().toStdString();
        juce::Logger::writeToLog("[HardwareContractRegistry] " + juce::String(lastErrorMessage));
        return false;
    }

    std::vector<HardwareContract> loadedContracts;

    for (const auto& file : files)
    {
        // ── LA CUARENTENA SE MIRA ANTES DE PARSEAR ──
        //
        // Y antes, y no despues de cargar, por una razon que no es de
        // rendimiento. Un contrato retenido que se parsesa y luego se tira es un
        // contrato que ha pasado por todo el codigo de construccion: ha creado
        // sus 31 funciones, sus controles y sus recetas de medicion, y eso ya
        // es trabajo hecho para algo que no se va a mostrar. Y si manana se
        // decide levantarla, ese trabajo ya esta probado.
        //
        // El aviso sale por el MISMO camino que los demas —`warnings`, el log y
        // `onProfileWarning`— porque un camino nuevo es un camino que nadie
        // mira. Se diferencia del resto en que aqui no es un fallo: es una
        // decision, y por eso va con su motivo.

        // La regla no se decide aqui. Se pide a `quarantine::evaluar`, que es el
        // unico sitio donde vive, para que el `SharedHardwareContractAdapter` no
        // pueda aplicar otra distinta sin que se note.
        const auto veredicto = quarantine::evaluar(file);

        if (veredicto.retenido)
        {
            // El `file` se guarda entero, no solo su nombre. Sin el path, quien
            // lee este aviso no puede ni abrir el JSON ni decir donde esta, y
            // levantar la retencion se convierte en cazar un fichero por un
            // arbol de repositorios a ojo.
            quarantine::Retenido retenido {
                file,
                file.getFileNameWithoutExtension(),
                veredicto.motivo,
            };

            quarantinedProfiles.push_back(retenido);

            // El aviso y la linea de log son COSA DISTINTA a proposito, y no
            // porque se hayan escrito con descuido.
            //
            // El aviso va al canal de avisos, que es donde se lee sin saber que
            // se va a leer: por eso lleva el texto entero y la frase que
            // distingue una decision de un fallo. Y el log lleva el PREFIJO
            // `[CUARENTENA]`, para que un grep por el distinga de las lineas
            // de error de verdad de un vistazo. Con el prefijo de siempre --
            // `[HardwareContractRegistry]`-- las dos cosas salian con la misma
            // etiqueta, y en un log de arranque eso es indistinguible.
            const juce::String aviso = quarantine::descripcionDeRetenido(retenido);

            warnings.push_back(aviso);
            juce::Logger::writeToLog(quarantine::lineaDeLogDeRetencion(retenido));

            if (onProfileWarning)
                onProfileWarning(aviso);

            continue;
        }

        HardwareContract contract;
        juce::String warning;
        if (loadProfileResilient(file, contract, warning))
        {
            // Un `status` que no sea `quarantined` se carga, pero se dice. La
            // regla es "retiene solo el literal conocido", y un valor
            // desconocido tiene que dejar rastro: si manana el esquema admite
            // `deprecated` y alguien lo escribe, el contrato aparecera en el
            // cajon sin que nadie haya decidido que eso es lo correcto.
            const auto estado = contract.status;

            if (!estado.empty() && estado != "quarantined")
            {
                const juce::String aviso = "Contrato '" + juce::String(contract.id)
                                           + "' lleva status '" + estado
                                           + "', que este registro no conoce: se carga igual.";

                warnings.push_back(aviso);
                juce::Logger::writeToLog("[HardwareContractRegistry] " + aviso);

                if (onProfileWarning)
                    onProfileWarning(aviso);
            }

            loadedContracts.push_back(std::move(contract));
        }
        else
        {
            invalidLegacyProfiles.insert(file.getFileNameWithoutExtension().toStdString());
            if (warning.isNotEmpty())
            {
                warnings.push_back(warning);
                juce::Logger::writeToLog("[HardwareContractRegistry] " + warning);
                if (onProfileWarning)
                {
                    onProfileWarning(warning);
                }
            }
        }
    }

    // ── UN DIRECTORIO SOLO CON CONTRATOS RETENIDOS ES UNA CARGA CORRECTA ──
    //
    // Y esto lo fijo un test, no una opinion. Con la condicion de antes, un
    // directorio con un solo contrato retenido devolvia FALLO, y eso es mentira
    // en dos sitios: no se ha fallado al parsear nada, y `loadedContracts` vacio
    // no distingue "aqui no hay nada" de "aqui no he podido leer nada".
    //
    // La diferencia se nota en el consumidor: `MainContentComponent` usa este
    // retorno para decidir si sigue buscando en el directorio padre. Con un
    // directorio retenido dando fallo, la busqueda seguiria subiendo creyendo
    // que aqui no habia nada que leer, y acabaria cargando el catalogo de otro
    // sitio. Un withholding declarado no debe cambiar de directorio.
    //
    // Y `lastErrorMessage` NO se limpia en ese caso, porque si no no queda nada
    // que diga por que hay cero contratos. `hasContracts()` contesta false, que
    // es la respuesta honesta, y el motivo esta en `warnings`.
    if (!loadedContracts.empty() || !quarantinedProfiles.empty())
    {
        contracts = std::move(loadedContracts);
        rebuildEffectiveContracts();

        if (contracts.empty())
        {
            lastErrorMessage = "No contracts loaded: "
                               + std::to_string(quarantinedProfiles.size())
                               + " withheld by quarantine";
        }
        else
        {
            lastErrorMessage.clear();
        }

        return true;
    }

    lastErrorMessage = "Failed to parse valid contracts from directory: " + contractsDir.getFullPathName().toStdString();
    return false;
}

CanonicalTargetProfileLoadResult HardwareContractRegistry::loadCanonicalTargetProfiles(const juce::File& targetsDir)
{
    CanonicalTargetProfileLoadResult result;

    if (!targetsDir.isDirectory())
    {
        result.outcome = CanonicalTargetProfileLoadOutcome::TargetDirectoryMissing;
        result.diagnosticCodes.push_back("ERR_TARGET_DIRECTORY_MISSING");
        result.diagnosticMessages.push_back("Targets directory does not exist: " + targetsDir.getFullPathName().toStdString());
        return result;
    }

    auto files = targetsDir.findChildFiles(juce::File::findFiles, false, "*.target.json");
    if (files.isEmpty())
    {
        files = targetsDir.findChildFiles(juce::File::findFiles, false, "*.json");
    }

    result.profilesDiscovered = static_cast<std::size_t>(files.size());
    if (files.isEmpty())
    {
        result.outcome = CanonicalTargetProfileLoadOutcome::Loaded;
        return result;
    }

    profiling::TargetProfileService service;
    std::vector<profiling::TargetProfile> validatedProfiles;
    validatedProfiles.reserve(result.profilesDiscovered);

    // 1. Validar todos los perfiles de forma atómica
    for (const auto& file : files)
    {
        auto valResult = service.loadAndValidateProfile(file);
        if (!valResult.isSuccess())
        {
            std::string errorMsg;
            for (const auto& diag : valResult.diagnostics)
            {
                if (diag.severity == profiling::DiagnosticSeverity::Error)
                {
                    if (!errorMsg.empty()) errorMsg += "; ";
                    errorMsg += diag.toString();
                }
            }
            if (errorMsg.empty()) errorMsg = "Validation failed";

            invalidCanonicalProfiles[file.getFileNameWithoutExtension().toStdString()] = errorMsg;
            result.outcome = CanonicalTargetProfileLoadOutcome::TargetProfileInvalid;
            result.diagnosticCodes.push_back("ERR_CANONICAL_TARGET_PROFILE_INVALID");
            result.diagnosticMessages.push_back(file.getFileName().toStdString() + ": " + errorMsg);
            return result; // RegistryUnchanged
        }
        validatedProfiles.push_back(valResult.profile);
    }

    // 2. Construir contenedores de ensayo (staging)
    std::vector<HardwareContract> stagedContracts;
    std::unordered_map<std::string, std::size_t> stagedCanonicalIdToIndex;
    std::unordered_map<std::string, std::string> stagedAliasToCanonicalId;

    const auto& certifiedParityLegacyIds = getCertifiedParityLegacyIds();

    for (const auto& profile : validatedProfiles)
    {
        const std::string& canId = profile.targetProfileId;

        // Comprobar colisión de ID canónico duplicado en el lote
        if (stagedCanonicalIdToIndex.find(canId) != stagedCanonicalIdToIndex.end())
        {
            result.outcome = CanonicalTargetProfileLoadOutcome::CanonicalAliasCollision;
            result.diagnosticCodes.push_back("ERR_CANONICAL_ID_DUPLICATE");
            result.diagnosticMessages.push_back("Duplicate canonical profile ID: " + canId);
            return result;
        }

        // Comprobar si canId coincide con un perfil legacy no certificado
        for (const auto& leg : contracts)
        {
            if (leg.id == canId && certifiedParityLegacyIds.find(leg.id) == certifiedParityLegacyIds.end())
            {
                result.outcome = CanonicalTargetProfileLoadOutcome::CanonicalLegacyParityUnproven;
                result.diagnosticCodes.push_back("ERR_CANONICAL_LEGACY_PARITY_UNPROVEN");
                result.diagnosticMessages.push_back("Canonical ID '" + canId + "' collides with unproven legacy contract");
                return result;
            }
        }

        // Construir vista legacy adaptada
        HardwareContract adapted = profiling::TargetProfileLegacyAdapter::toLegacyHardwareContract(profile);
        stagedContracts.push_back(adapted);
        std::size_t idx = stagedContracts.size() - 1;
        stagedCanonicalIdToIndex[canId] = idx;

        // Validar y registrar aliases (acceptedUniqueIds)
        for (const auto& alias : profile.identity.acceptedUniqueIds)
        {
            if (alias == canId)
                continue; // auto-alias consistente

            // Un alias apunta a dos perfiles canónicos distintos
            auto itAlias = stagedAliasToCanonicalId.find(alias);
            if (itAlias != stagedAliasToCanonicalId.end() && itAlias->second != canId)
            {
                result.outcome = CanonicalTargetProfileLoadOutcome::CanonicalAliasCollision;
                result.diagnosticCodes.push_back("ERR_CANONICAL_ALIAS_COLLISION");
                result.diagnosticMessages.push_back("Alias '" + alias + "' claimed by multiple canonical profiles");
                return result;
            }

            // Un alias coincide con el ID canónico de otro perfil
            if (stagedCanonicalIdToIndex.find(alias) != stagedCanonicalIdToIndex.end() && alias != canId)
            {
                result.outcome = CanonicalTargetProfileLoadOutcome::CanonicalAliasCollision;
                result.diagnosticCodes.push_back("ERR_CANONICAL_ALIAS_COLLIDES_WITH_CANONICAL_ID");
                result.diagnosticMessages.push_back("Alias '" + alias + "' collides with another canonical ID");
                return result;
            }

            // Un alias coincide con un contrato legacy no certificado
            for (const auto& leg : contracts)
            {
                if (leg.id == alias && certifiedParityLegacyIds.find(leg.id) == certifiedParityLegacyIds.end())
                {
                    result.outcome = CanonicalTargetProfileLoadOutcome::CanonicalLegacyParityUnproven;
                    result.diagnosticCodes.push_back("ERR_CANONICAL_LEGACY_PARITY_UNPROVEN");
                    result.diagnosticMessages.push_back("Alias '" + alias + "' collides with unproven legacy contract: " + leg.id);
                    return result;
                }
            }

            stagedAliasToCanonicalId[alias] = canId;
        }
    }

    // Registrar aliases legacy para pares con paridad certificada
    for (const auto& pair : getCertifiedParityPairs())
    {
        if (stagedCanonicalIdToIndex.find(pair.canonicalId) != stagedCanonicalIdToIndex.end())
        {
            stagedAliasToCanonicalId[pair.legacyId] = pair.canonicalId;
        }
    }

    // 3. Compromiso atómico (Atomic Commit)
    canonicalAdaptedContracts = std::move(stagedContracts);
    canonicalIdToIndex = std::move(stagedCanonicalIdToIndex);
    aliasToCanonicalId = std::move(stagedAliasToCanonicalId);

    rebuildEffectiveContracts();

    result.outcome = CanonicalTargetProfileLoadOutcome::Loaded;
    result.profilesLoaded = canonicalAdaptedContracts.size();
    result.diagnosticCodes.push_back("INFO_CANONICAL_PROFILES_LOADED");
    result.diagnosticMessages.push_back("Successfully loaded " + std::to_string(result.profilesLoaded) + " canonical target profiles.");
    return result;
}

void HardwareContractRegistry::rebuildEffectiveContracts()
{
    effectiveContracts.clear();
    effectiveContracts.reserve(contracts.size() + canonicalAdaptedContracts.size());

    const auto& certifiedLegacyIds = getCertifiedParityLegacyIds();

    // 1. Agregar contratos legacy no superados por canónicos adaptados
    for (const auto& leg : contracts)
    {
        bool superseded = false;
        if (certifiedLegacyIds.find(leg.id) != certifiedLegacyIds.end())
        {
            for (const auto& pair : getCertifiedParityPairs())
            {
                if (pair.legacyId == leg.id)
                {
                    if (canonicalIdToIndex.find(pair.canonicalId) != canonicalIdToIndex.end())
                    {
                        superseded = true;
                    }
                    break;
                }
            }
        }
        if (!superseded)
        {
            effectiveContracts.push_back(leg);
        }
    }

    // 2. Agregar contratos canónicos adaptados con precedencia
    for (const auto& can : canonicalAdaptedContracts)
    {
        effectiveContracts.push_back(can);
    }
}

HardwareContractResolution HardwareContractRegistry::resolveContractById(const std::string& id) const
{
    HardwareContractResolution res;

    // 0. Si se solicita un perfil canónico que se sabe inválido, bloquear sin fallback
    auto itInv = invalidCanonicalProfiles.find(id);
    if (itInv != invalidCanonicalProfiles.end())
    {
        res.contract = std::nullopt;
        res.source = HardwareContractResolutionSource::CanonicalProfileInvalid;
        res.diagnosticCode = "ERR_CANONICAL_TARGET_PROFILE_INVALID";
        res.diagnosticMessage = itInv->second;
        return res;
    }

    const auto& certifiedParityLegacyIds = getCertifiedParityLegacyIds();

    // 1. Buscar en perfiles canónicos adaptados (por ID canónico o por acceptedUniqueId / alias)
    std::string canonicalId;
    auto itId = canonicalIdToIndex.find(id);
    if (itId != canonicalIdToIndex.end())
    {
        canonicalId = id;
    }
    else
    {
        auto itAlias = aliasToCanonicalId.find(id);
        if (itAlias != aliasToCanonicalId.end())
        {
            canonicalId = itAlias->second;
        }
    }

    if (!canonicalId.empty())
    {
        std::size_t index = canonicalIdToIndex.at(canonicalId);
        const auto& canonicalContract = canonicalAdaptedContracts[index];

        // Verificar si existe colisión en los contratos legacy nativos
        const HardwareContract* nativeLegacy = nullptr;
        std::string certifiedLegacyId;
        for (const auto& pair : getCertifiedParityPairs())
        {
            if (pair.canonicalId == canonicalId)
            {
                certifiedLegacyId = pair.legacyId;
                break;
            }
        }

        for (const auto& leg : contracts)
        {
            if (leg.id == id || leg.id == canonicalId || (!certifiedLegacyId.empty() && leg.id == certifiedLegacyId))
            {
                nativeLegacy = &leg;
                break;
            }
            auto itA = aliasToCanonicalId.find(leg.id);
            if (itA != aliasToCanonicalId.end() && itA->second == canonicalId)
            {
                nativeLegacy = &leg;
                break;
            }
        }

        if (nativeLegacy != nullptr)
        {
            // Existen ambos: verificar si es uno de los tres con paridad certificada
            if (certifiedParityLegacyIds.find(nativeLegacy->id) != certifiedParityLegacyIds.end())
            {
                res.contract = canonicalContract;
                res.source = HardwareContractResolutionSource::CanonicalTargetProfileAdapted;
                res.diagnosticCode = "INFO_CANONICAL_LEGACY_PARITY_CERTIFIED";
                res.diagnosticMessage = "Resolved canonical profile with certified legacy parity for id: " + id;
                return res;
            }
            else
            {
                // Colisión no certificada -> Fail-Closed
                res.contract = std::nullopt;
                res.source = HardwareContractResolutionSource::CanonicalLegacyParityViolation;
                res.diagnosticCode = "ERR_CANONICAL_LEGACY_PARITY_UNPROVEN";
                res.diagnosticMessage = "Unproven parity collision between canonical and legacy for id: " + id;
                return res;
            }
        }

        // Si legacy existe pero fue inválido
        if (invalidLegacyProfiles.find(id) != invalidLegacyProfiles.end())
        {
            res.contract = canonicalContract;
            res.source = HardwareContractResolutionSource::CanonicalTargetProfileAdapted;
            res.diagnosticCode = "WARN_LEGACY_PROFILE_INVALID_CANONICAL_USED";
            res.diagnosticMessage = "Legacy profile was invalid; resolved certified canonical profile for id: " + id;
            return res;
        }

        // Solo existe canónico adaptado
        res.contract = canonicalContract;
        res.source = HardwareContractResolutionSource::CanonicalTargetProfileAdapted;
        res.diagnosticCode = "INFO_CANONICAL_TARGET_PROFILE_ADAPTED";
        res.diagnosticMessage = "Resolved canonical profile adapted for id: " + id;
        return res;
    }

    // 2. Si no existe canónico, buscar en contratos legacy nativos
    for (const auto& leg : contracts)
    {
        if (leg.id == id)
        {
            res.contract = leg;
            res.source = HardwareContractResolutionSource::NativeLegacyContract;
            res.diagnosticCode = "INFO_NATIVE_LEGACY_CONTRACT";
            res.diagnosticMessage = "Resolved native legacy contract for id: " + id;
            return res;
        }
    }

    // 3. No existe en ninguno
    if (invalidLegacyProfiles.find(id) != invalidLegacyProfiles.end())
    {
        res.contract = std::nullopt;
        res.source = HardwareContractResolutionSource::LegacyProfileInvalid;
        res.diagnosticCode = "ERR_LEGACY_PROFILE_INVALID";
        res.diagnosticMessage = "Legacy profile was invalid for id: " + id;
        return res;
    }

    res.contract = std::nullopt;
    res.source = HardwareContractResolutionSource::NotFound;
    res.diagnosticCode = "ERR_HARDWARE_CONTRACT_NOT_FOUND";
    res.diagnosticMessage = "Hardware contract not found for id: " + id;
    return res;
}

const HardwareContract* HardwareContractRegistry::findContractById(const std::string& id) const noexcept
{
    auto resolution = resolveContractById(id);
    if (!resolution.contract.has_value())
        return nullptr;

    if (resolution.source == HardwareContractResolutionSource::CanonicalTargetProfileAdapted)
    {
        auto it = canonicalIdToIndex.find(resolution.contract->id);
        if (it != canonicalIdToIndex.end() && it->second < canonicalAdaptedContracts.size())
            return &canonicalAdaptedContracts[it->second];
    }
    else if (resolution.source == HardwareContractResolutionSource::NativeLegacyContract)
    {
        for (const auto& c : contracts)
        {
            if (c.id == resolution.contract->id)
                return &c;
        }
    }

    return nullptr;
}

} // namespace abdaudiolab::core
