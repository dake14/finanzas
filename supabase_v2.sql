-- ============================================================
-- supabase_v2.sql — las tablas del modelo nuevo.
--
-- CORRELO VOS, en el panel de Supabase: SQL Editor > New query > pegar > Run.
-- No lo corre la aplicacion ni ningun agente: es una migracion contra tu
-- proyecto real y el unico que decide cuando tocarlo sos vos.
--
-- Es seguro correrlo mas de una vez: todo es "if not exists" y las politicas se
-- borran antes de recrearse.
--
-- NO TOCA la tabla `movements` de la aplicacion vieja. Las nuevas llevan
-- prefijo v2_ justamente para poder convivir mientras descontinuas aquella.
-- Cuando la borres, `drop table public.movements;` y listo.
-- ============================================================


-- ------------------------------------------------------------
-- 1. Bolsillos
-- ------------------------------------------------------------
-- Un bolsillo es un LUGAR con saldo, no un porcentaje de una resta. Que exista
-- esta tabla es la diferencia de fondo con el modelo viejo: sin ella no hay
-- traspaso, y sin traspaso "saque del ahorro para comprar material" no se
-- puede escribir.
create table if not exists public.v2_pockets (
    id            uuid primary key,
    user_id       uuid not null references auth.users(id) on delete cascade,

    name          text        not null,
    kind          text        not null,

    -- Entero en centavos. NUNCA numeric ni float: un redondeo binario mal
    -- hecho descuadra los saldos de forma permanente.
    opening_minor bigint      not null default 0,
    archived      boolean     not null default false,

    hlc           text        not null,
    device_id     text        not null,
    deleted       boolean     not null default false,
    updated_at    timestamptz not null default now()
);


-- ------------------------------------------------------------
-- 2. Trabajos
-- ------------------------------------------------------------
create table if not exists public.v2_jobs (
    id         uuid primary key,
    user_id    uuid not null references auth.users(id) on delete cascade,

    name       text        not null,
    client     text        not null default '',
    opened     date        not null,
    closed     boolean     not null default false,

    hlc        text        not null,
    device_id  text        not null,
    deleted    boolean     not null default false,
    updated_at timestamptz not null default now()
);


-- ------------------------------------------------------------
-- 3. Movimientos
-- ------------------------------------------------------------
create table if not exists public.v2_movements (
    id               uuid primary key,
    user_id          uuid not null references auth.users(id) on delete cascade,

    date             date        not null,
    name             text        not null,
    kind             text        not null check (kind in ('Ingreso', 'Gasto', 'Traspaso')),
    amount_minor     bigint      not null,

    -- De donde sale la plata y, si es traspaso, a donde va.
    --
    -- Sin FOREIGN KEY a proposito. Las filas llegan por sincronizacion y no hay
    -- garantia de orden entre dos equipos: si un movimiento llegara antes que
    -- su bolsillo, una clave foranea lo rechazaria y ese movimiento se perderia
    -- para siempre. El orden lo cuida el cliente, que sube y baja bolsillos,
    -- despues trabajos y despues movimientos.
    pocket_id        uuid        not null,
    target_pocket_id uuid,

    category         text        not null default '',
    job_id           uuid,

    -- En cuantos meses se reparte el costo. Cuatro kilos de filamento mueven la
    -- caja hoy y el resultado durante cuatro meses.
    spread_months    integer     not null default 1 check (spread_months >= 1),

    -- Falso = entregado y todavia sin cobrar.
    settled          boolean     not null default true,

    recurrence       text        not null default 'Puntual'
                                 check (recurrence in ('Puntual', 'Mensual')),

    hlc              text        not null,
    device_id        text        not null,
    deleted          boolean     not null default false,
    updated_at       timestamptz not null default now()
);


-- ------------------------------------------------------------
-- 4. Indices
-- ------------------------------------------------------------
-- El motor baja "todo lo que cambio despues de X" por tabla, asi que estos
-- indices son los que sostienen la consulta principal de cada una.
create index if not exists v2_pockets_user_updated_idx
    on public.v2_pockets (user_id, updated_at);

create index if not exists v2_jobs_user_updated_idx
    on public.v2_jobs (user_id, updated_at);

create index if not exists v2_movements_user_updated_idx
    on public.v2_movements (user_id, updated_at);

create index if not exists v2_movements_user_date_idx
    on public.v2_movements (user_id, date);


-- ------------------------------------------------------------
-- 5. updated_at se pone solo
-- ------------------------------------------------------------
-- El cliente NO manda updated_at: si lo mandara, un equipo con el reloj
-- atrasado podria escribir una marca anterior a la del cursor de otro y ese
-- cambio no bajaria nunca. La hora del servidor es la unica que todos comparten.
create or replace function public.v2_touch_updated_at()
returns trigger language plpgsql as $$
begin
    new.updated_at = now();
    return new;
end;
$$;

drop trigger if exists v2_pockets_touch on public.v2_pockets;
create trigger v2_pockets_touch before insert or update on public.v2_pockets
    for each row execute function public.v2_touch_updated_at();

drop trigger if exists v2_jobs_touch on public.v2_jobs;
create trigger v2_jobs_touch before insert or update on public.v2_jobs
    for each row execute function public.v2_touch_updated_at();

drop trigger if exists v2_movements_touch on public.v2_movements;
create trigger v2_movements_touch before insert or update on public.v2_movements
    for each row execute function public.v2_touch_updated_at();


-- ------------------------------------------------------------
-- 6. Seguridad por fila (RLS)
-- ------------------------------------------------------------
-- ESTO es lo que protege los datos, no que la clave publicable sea secreta.
-- La clave viaja dentro del APK y es publica por diseño; sin estas politicas,
-- cualquiera que la lea se lleva tus finanzas.
alter table public.v2_pockets   enable row level security;
alter table public.v2_jobs      enable row level security;
alter table public.v2_movements enable row level security;

do $$
declare t text;
begin
    foreach t in array array['v2_pockets', 'v2_jobs', 'v2_movements'] loop
        execute format('drop policy if exists "lectura propia" on public.%I', t);
        execute format('drop policy if exists "alta propia" on public.%I', t);
        execute format('drop policy if exists "edicion propia" on public.%I', t);

        execute format($f$
            create policy "lectura propia" on public.%I
                for select using (auth.uid() = user_id)
        $f$, t);

        execute format($f$
            create policy "alta propia" on public.%I
                for insert with check (auth.uid() = user_id)
        $f$, t);

        execute format($f$
            create policy "edicion propia" on public.%I
                for update using (auth.uid() = user_id)
                           with check (auth.uid() = user_id)
        $f$, t);
    end loop;
end $$;

-- No hay politica de DELETE, y es a proposito: nada se borra fisicamente. Un
-- borrado es una lapida (deleted = true), que es lo unico que se puede
-- sincronizar; un DELETE de verdad reaparece en el otro equipo en la proxima
-- bajada.
