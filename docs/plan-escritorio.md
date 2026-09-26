# Plan: Finanzas DakeLabs, escritorio

Spec: documento de diseño "Finanzas DakeLabs — diseño del escritorio"
(https://claude.ai/code/artifact/73c61346-72c6-4495-a876-71b6495cd07b), revisado el 2026-09-25.

## Restricciones globales

- `dake::core` sigue sin Qt: solo STL. Toda fórmula nueva va ahí y se prueba en `tests/negociotest.cpp`
  (ctest `negocio`).
- Importes en centavos (`int64`), porcentajes en puntos básicos, horas en minutos. Ninguna columna `REAL`.
- **Nada nuevo viaja en la sincronización.** La cola de salida sube el JSON tal cual y PostgREST rechaza las
  columnas que no conoce, así que una columna nueva en `pockets`, `jobs` o `movements` frenaría la
  sincronización del teléfono hasta correr SQL en el panel. Todo dato nuevo vive en tablas locales
  (`*_meta`, `repairs`, etc.) unidas por id. Los movimientos que se crean solos (cobros, recurrentes,
  importados) sí son movimientos normales y sincronizan.
- El esquema pasa a la versión 3 con todas las tablas nuevas de una vez, con respaldo antes de migrar (ya
  existe).
- `compilar.ps1 debug -Probar` tiene que pasar entero al final de cada etapa. Cero advertencias.
- La app del teléfono no se toca.

## Tareas

1. **Cuentas.** `Account` en `Pocket` (default por tipo, override en `pocket_meta`); `Category` con cuenta y
   clase; sueldo = traspaso negocio → personal; gasto cruzado → traspaso + gasto personal; gastos por
   categoría y cuenta contra el mes anterior. Esquema v3. UI: cuenta en Bolsillos; categorías; reporte 4.
2. **Captura.** Intérprete `parseCapture` (monto, tipo, fecha, reparación, categoría, bolsillo, descripción)
   con aprendizaje desde el historial; ventana mini; bandeja del sistema; atajo global `Ctrl+Alt+Espacio`;
   instancia única (`--anotar`); arranque con Windows; cronómetro de capturas.
3. **Reparaciones.** `Repair`, `RepairPart`, `RepairTemplate`; costo, margen, ganancia por hora, precio
   sugerido; estadísticas por tipo. UI: página Reparaciones, `Ctrl+R` desde plantilla, entregar/cobrar con su
   ingreso; Ajustes (tarifa, margen, reparto, plantillas); reportes 2 y 3.
4. **Cotizaciones.** Lectura de `Documentos\DakeLabs Cotizaciones`; plan puro de acciones por documento;
   ids deterministas; conciliación inicial con detección de duplicados; vigilancia de la carpeta.
5. **Fijos y bandeja.** Recurrentes (generación idempotente por mes), herramientas (depreciación), tasa de
   fijos por hora, bandeja de pendientes, pantalla de revisión, recordatorio del domingo.
6. **Sueldo y caja.** Utilidad neta mensual, sueldo recomendado, reparto; reportes 1 y 5; pantalla Hoy.
7. **Bancos.** CSV con perfiles por banco, huella anti-duplicados, cruce con lo anotado, categoría sugerida.
