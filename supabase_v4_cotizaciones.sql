-- ============================================================
-- supabase_v4_cotizaciones.sql — lo esencial de DakeLabs Cotizaciones.
--
-- CORRELO VOS en el panel: SQL Editor > New query > pegar > Run.
-- Es seguro correrlo más de una vez y no toca ninguna tabla que ya exista.
--
-- Una fila por documento. Solo lo que hace falta para verlo en el teléfono y
-- para que Finanzas lo importe; lo pesado (descripciones, cláusulas, notas,
-- datos del cliente, fotos) se queda en la PC.
--
-- Dos grupos de columnas, cada uno con su reloj:
--   contenido (hlc): lo escribe solo el dueño del documento;
--   estado (estado_hlc): lo escriben la PC y el teléfono.
-- Todas tienen valor por defecto: el teléfono sube solo el grupo que cambió.
-- ============================================================

create table if not exists public.v2_quotes (
    id              text primary key,
    user_id         uuid not null references auth.users(id) on delete cascade,

    numero          text    not null default '',
    tipo            text    not null default 'cotizacion' check (tipo in ('cotizacion', 'informe')),
    forma           text    not null default 'servicio'   check (forma in ('servicio', 'proyecto')),
    cliente         text    not null default '',
    equipo          text    not null default '',
    lineas          jsonb   not null default '[]'::jsonb,
    descuento_tipo  text    not null default '' check (descuento_tipo in ('', 'porcentaje', 'monto')),
    descuento_valor bigint  not null default 0,
    abono_minor     bigint  not null default 0,
    fecha_emision   text    not null default '',
    fecha_ingreso   text    not null default '',
    origen_id       text    not null default '',
    hecho_en        text    not null default 'pc' check (hecho_en in ('pc', 'telefono')),
    hlc             text    not null default '',

    estado          text    not null default 'borrador',
    fecha_entrega   text    not null default '',
    fecha_pago      text    not null default '',
    motivo_rechazo  text    not null default '',
    estado_hlc      text    not null default '',

    device_id       text    not null default '',
    deleted         boolean not null default false,
    updated_at      timestamptz not null default now()
);

create index if not exists v2_quotes_user_updated_idx
    on public.v2_quotes (user_id, updated_at);

drop trigger if exists v2_quotes_touch on public.v2_quotes;
create trigger v2_quotes_touch before insert or update on public.v2_quotes
    for each row execute function public.v2_touch_updated_at();

alter table public.v2_quotes enable row level security;

drop policy if exists "lectura propia" on public.v2_quotes;
drop policy if exists "alta propia" on public.v2_quotes;
drop policy if exists "edicion propia" on public.v2_quotes;

create policy "lectura propia" on public.v2_quotes
    for select using (auth.uid() = user_id);
create policy "alta propia" on public.v2_quotes
    for insert with check (auth.uid() = user_id);
create policy "edicion propia" on public.v2_quotes
    for update using (auth.uid() = user_id) with check (auth.uid() = user_id);

-- Sin política de DELETE: igual que las demás, un borrado es una lápida.
