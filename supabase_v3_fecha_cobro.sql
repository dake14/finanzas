-- ============================================================
-- supabase_v3_fecha_cobro.sql — cuándo se cobró, no solo si se cobró.
--
-- CORRELO VOS en el panel: SQL Editor > New query > pegar > Run.
--
-- POR QUE
--
-- El modelo tenía `settled`, un sí/no de "ya lo cobré". Con eso se puede saber
-- cuánto te deben, pero no cuánto TARDAS en cobrar — que es el número que
-- separa un negocio rentable de uno rentable en el papel y sin plata en la
-- caja. Para eso hace falta la fecha.
--
-- Va NULA por defecto, y esa nulidad es información y no un hueco: significa
-- "no se sabe", que es la verdad de todo lo anotado antes de hoy. El promedio
-- de días de cobro saltea esas filas en vez de inventarles una fecha, porque
-- una fecha inventada ensucia el promedio sin que nadie se entere.
--
-- Es seguro correrlo más de una vez: `if not exists`.
-- ============================================================

alter table public.v2_movements
    add column if not exists settled_date text not null default '';

-- Sin índice. Esta columna no se filtra ni se ordena por ella: se lee entera
-- junto con el resto de la fila. Un índice que nadie usa cuesta escritura en
-- cada sincronización y no devuelve nada.
