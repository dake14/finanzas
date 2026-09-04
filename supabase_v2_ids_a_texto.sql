-- ============================================================
-- supabase_v2_ids_a_texto.sql — arreglo del tipo de los identificadores.
--
-- CORRELO VOS en el panel: SQL Editor > New query > pegar > Run.
--
-- POR QUE EXISTE ESTE ARCHIVO
--
-- La primera version de supabase_v2.sql declaro los identificadores como
-- `uuid`. Fue un error de diseño: el modelo de la aplicacion define `Id` como
-- una cadena opaca, no como un uuid, y la siembra usa ids legibles —'p-caja',
-- 'j-macbook', 'm-01'— que se entienden leyendo una consulta.
--
-- El sintoma era que sincronizar fallaba entero con
--     400  22P02  invalid input syntax for type uuid: "p-caja"
-- y como Postgres rechaza el lote COMPLETO, no subia ni una fila.
--
-- No borra nada y no pierde datos: solo cambia el tipo de las columnas. Si las
-- tablas ya tuvieran filas, los valores se conservan tal cual.
--
-- `user_id` NO se toca: ese si es un uuid de verdad, el de auth.users, y es
-- contra el que evaluan las politicas RLS.
-- ============================================================

alter table public.v2_pockets
    alter column id type text;

alter table public.v2_jobs
    alter column id type text;

alter table public.v2_movements
    alter column id        type text,
    alter column pocket_id type text;

-- Estas dos eran uuid anulable. Pasan a texto con cadena vacia por defecto,
-- que es exactamente como el modelo local representa "sin trabajo" y "sin
-- bolsillo destino". Asi el dato viaja identico por la nube y por el archivo
-- de exportar, sin traducciones a null que haya que recordar en los dos
-- sentidos.
alter table public.v2_movements
    alter column target_pocket_id type text using coalesce(target_pocket_id::text, ''),
    alter column job_id           type text using coalesce(job_id::text, '');

alter table public.v2_movements
    alter column target_pocket_id set default '',
    alter column job_id           set default '';

alter table public.v2_movements
    alter column target_pocket_id set not null,
    alter column job_id           set not null;
