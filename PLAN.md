# ABDAudioLab — Plan de Ejecución Automática

## Objetivo
Implementar los métodos de forwarding de estado de puntos en `SoundIdSuiteList.cpp` para completar la integración de la Fase 14 con `modelManager`.

## Archivos a modificar
- `src/gui/SoundIdSuiteList.cpp`

## Instrucciones específicas

En `src/gui/SoundIdSuiteList.cpp`, añade al final del archivo las siguientes tres funciones públicas delegando en `modelManager`:

```cpp
void SoundIdSuiteList::setPointStatus(int queueIndex, int pointIndex, PointStatus status)
{
    modelManager.setPointStatus(queueIndex, pointIndex, status);
    layoutRows();
    rowsContent.repaint();
}

PointStatus SoundIdSuiteList::getPointStatus(int queueIndex, int pointIndex) const
{
    return modelManager.getPointStatus(queueIndex, pointIndex);
}

void SoundIdSuiteList::resetPointStatuses(int queueIndex)
{
    modelManager.resetPointStatuses(queueIndex);
    layoutRows();
    rowsContent.repaint();
}
```

## Verificación
El comando de test automático (`cmd /c build.bat`) se ejecutará solo tras guardar los cambios.
Una vez compila con éxito, Aider realizará el commit automáticamente.
