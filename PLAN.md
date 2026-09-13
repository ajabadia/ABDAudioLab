# Plan de Trabajo: Fase 20.3.C — Segundo Target Externo (Plugin Abierto del Ecosistema VST3)

## Objetivo
Evaluar el pipeline completo de ABDAudioLab contra un sintetizador VST3 de terceros (de código abierto o ejemplo oficial de referencia del VST3 SDK), certificando la honestidad diagnóstica del sistema frente a anomalías reales (semántica ambigua, suavizado no declarado, parámetros inertes, persistencia de fase o estado parcial).

## Criterios Clave
1. No forzar un dictamen `Approved`: certificar que `ApprovedWithWarnings` prescribe las adaptaciones operativas adecuadas.
2. Identificación del bundle y componentes con `PluginIdentity` y hashes canónicos.
3. Desacoplamiento entre lo que el host descubre y lo que el observador acústico mide.
4. Generación de los 10 artefactos reproducibles en el arnés CLI.
