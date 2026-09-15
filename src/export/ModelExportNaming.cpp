#include "ModelExportNaming.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace abdaudiolab::exporting
{

namespace
{

const std::vector<std::string>& getWindowsReservedNames()
{
    static const std::vector<std::string> reserved = {
        "CON", "PRN", "AUX", "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
    };
    return reserved;
}

} // namespace

std::string ModelExportNaming::sanitizeComponent(const std::string& input,
                                                 const std::string& fallbackDefault,
                                                 size_t maxLength)
{
    if (input.empty())
        return fallbackDefault;

    std::string out;
    out.reserve(input.size());

    for (char c : input)
    {
        unsigned char uc = static_cast<unsigned char>(c);
        // Caracteres prohibidos en Windows y POSIX
        if (uc < 32 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c == ' ' || c == '(' || c == ')' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == ';' || c == ',' || c == '\'' ||
            c == '`' || c == '~' || c == '!' || c == '@' || c == '#' ||
            c == '$' || c == '%' || c == '^' || c == '&' || c == '=')
        {
            out.push_back('_');
        }
        else if (c == '-')
        {
            out.push_back('_');
        }
        else
        {
            out.push_back(c);
        }
    }

    // Colapsar guiones bajos consecutivos "__" -> "_"
    std::string collapsed;
    collapsed.reserve(out.size());
    bool lastWasUnderscore = false;
    for (char c : out)
    {
        if (c == '_')
        {
            if (!lastWasUnderscore)
            {
                collapsed.push_back(c);
                lastWasUnderscore = true;
            }
        }
        else
        {
            collapsed.push_back(c);
            lastWasUnderscore = false;
        }
    }

    // Recortar guiones bajos al inicio y al final
    size_t first = collapsed.find_first_not_of('_');
    if (first == std::string::npos)
        return fallbackDefault;

    size_t last = collapsed.find_last_not_of('_');
    std::string trimmed = collapsed.substr(first, (last - first + 1));

    // Comprobar nombres reservados de Windows (case-insensitive)
    std::string upperTrimmed = trimmed;
    std::transform(upperTrimmed.begin(), upperTrimmed.end(), upperTrimmed.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    for (const auto& res : getWindowsReservedNames())
    {
        if (upperTrimmed == res)
        {
            trimmed = "_" + trimmed;
            break;
        }
    }

    // Limitar longitud máxima
    if (maxLength > 0 && trimmed.size() > maxLength)
    {
        trimmed = trimmed.substr(0, maxLength);
        // Asegurar que no termine en guión bajo tras el truncado
        while (!trimmed.empty() && trimmed.back() == '_')
            trimmed.pop_back();
        if (trimmed.empty())
            trimmed = fallbackDefault;
    }

    return trimmed;
}

std::string ModelExportNaming::formatUtcTimestamp(const juce::Time& time)
{
    // Formato ISO-8601 compacto en UTC estricto: YYYYMMDDTHHMMSSZ
    const auto millis = time.toMilliseconds();
    const time_t t = static_cast<time_t>(millis / 1000);
    struct tm tmUtc{};
#if defined(_WIN32)
    gmtime_s(&tmUtc, &t);
#else
    gmtime_r(&t, &tmUtc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tmUtc);
    return std::string(buf);
}

std::string ModelExportNaming::extractHashPrefix(const std::string& fullCanonicalHash, size_t prefixLength)
{
    if (fullCanonicalHash.empty())
        return "00000000";

    std::string sanitized;
    sanitized.reserve(fullCanonicalHash.size());
    for (char c : fullCanonicalHash)
    {
        if (std::isxdigit(static_cast<unsigned char>(c)))
        {
            sanitized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }

    if (sanitized.empty())
        return "00000000";

    if (sanitized.size() > prefixLength)
        return sanitized.substr(0, prefixLength);

    while (sanitized.size() < prefixLength)
        sanitized.push_back('0');

    return sanitized;
}

std::string ModelExportNaming::buildFileName(const std::string& targetName,
                                            const std::string& modelType,
                                            const juce::Time& utcTime,
                                            const std::string& fullCanonicalHash)
{
    std::string safeTarget = sanitizeComponent(targetName, "Target", 32);
    std::string safeModel = sanitizeComponent(modelType, "Model", 32);
    std::string timestamp = formatUtcTimestamp(utcTime);
    std::string hashPrefix = extractHashPrefix(fullCanonicalHash, 8);

    return safeTarget + "_" + safeModel + "_" + timestamp + "_" + hashPrefix + ".h";
}

juce::File ModelExportNaming::resolveUniqueExportFile(const juce::File& exportDir,
                                                    const std::string& baseFileName)
{
    juce::File candidate = exportDir.getChildFile(baseFileName);
    if (!candidate.existsAsFile())
        return candidate;

    // Extraer nombre base y extensión
    std::string fileName = baseFileName;
    std::string stem = fileName;
    std::string ext = ".h";

    size_t dotPos = fileName.rfind('.');
    if (dotPos != std::string::npos)
    {
        stem = fileName.substr(0, dotPos);
        ext = fileName.substr(dotPos);
    }

    // Buscar el siguiente sufijo libre _1, _2, _3...
    int counter = 1;
    while (candidate.existsAsFile())
    {
        std::string newName = stem + "_" + std::to_string(counter++) + ext;
        candidate = exportDir.getChildFile(newName);
    }

    return candidate;
}

} // namespace abdaudiolab::exporting
