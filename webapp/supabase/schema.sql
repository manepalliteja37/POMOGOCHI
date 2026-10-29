-- Pomogochi companion: run this once in Supabase → SQL Editor → New query → Run.

create table if not exists public.pets (
  user_id    uuid primary key references auth.users(id) on delete cascade,
  kind       text not null check (kind in ('dog','rabbit','parrot','plant')),
  name       text not null check (char_length(name) between 1 and 20),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create table if not exists public.day_sheets (
  user_id     uuid not null references auth.users(id) on delete cascade,
  date        date not null,
  s25         smallint not null default 0,
  s50         smallint not null default 0,
  done_count  smallint not null default 0,
  minutes     smallint not null default 0,
  growth      smallint not null default 0 check (growth between 0 and 8),
  streak      smallint not null default 0,
  raw         jsonb not null,
  uploaded_at timestamptz not null default now(),
  primary key (user_id, date)
);

alter table public.pets       enable row level security;
alter table public.day_sheets enable row level security;

-- each user sees and changes only their own rows
drop policy if exists "own pet" on public.pets;
create policy "own pet" on public.pets
  for all using (auth.uid() = user_id) with check (auth.uid() = user_id);

drop policy if exists "own sheets" on public.day_sheets;
create policy "own sheets" on public.day_sheets
  for all using (auth.uid() = user_id) with check (auth.uid() = user_id);
