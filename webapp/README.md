# 🐾 Pomogochi Companion

A web app for the **Pomodoro Rabbit** desk timer. Sign in with Google, pick a **dog, rabbit, parrot or plant**, and grow it by uploading the device's daily JSON day sheets.

- **Growth per day** = 25-min focus ×1 + 50-min focus ×2 + done tasks ×1, **max 8 a day**. 1/2/10-min test focuses don't count.
- **Stages:** Egg 0 → Baby 1 → Kid 10 → Teen 40 → Adult 100 total growth.
- Uploading the same day again **replaces** it (no double counting). You can upload many days at once.
- **Switching pets keeps all your growth.**

Stack: Next.js 15 (App Router, TypeScript) + Supabase (Google Auth + Postgres with row-level security), hosted on Vercel.

## Pages

| Page | What it does |
|---|---|
| `/` | Sign in with Google |
| `/onboarding` | First visit: pick a pet + name |
| `/home` | Your pet at its stage, growth bar, streak, today and last 7 days |
| `/upload` | Drag and drop day sheets → preview (date, focus, tasks, +growth) → Confirm |
| `/history` | 5-week calendar + a list of every uploaded day (delete a day) |
| `/settings` | Switch pet / rename, sign out, start over |

## Where the JSON comes from

Device web page (`http://pomodoro.local`) → **History** → tap **JSON** next to a day. That's `/api/day?d=YYYYMMDD` (format: `{"app":"pomodoro","sheet":1,"date":...,"focus":{s25,s50,minutes},"done":[...],"streak":...}`).
A full **Export** backup is not a day sheet, and the app tells you so.

---

## Setup (one time, about 20 minutes)

### 1. Supabase (database + login)
1. Go to supabase.com → **New project**. Region: **South Asia (Mumbai)**. Save the database password somewhere.
2. **SQL Editor → New query**: paste all of `supabase/schema.sql` → **Run**.
3. **Project Settings → API**: copy the **Project URL** and the **anon public** key.

### 2. Google sign-in
1. Go to console.cloud.google.com → create a project (for example "Pomogochi").
2. **APIs & Services → OAuth consent screen**: External, add an app name + your email, then **Publish app**.
3. **Credentials → Create credentials → OAuth client ID → Web application**.
   - Authorized redirect URI: `https://YOUR-PROJECT.supabase.co/auth/v1/callback` (Supabase shows this exact URL on its Google provider page)
4. Copy the **Client ID** and **Client secret**.
5. Supabase → **Authentication → Sign In / Providers → Google**: turn it on, then paste the ID + secret → Save.

### 3. Run it on your PC (optional)
```bash
cd webapp
cp .env.example .env.local      # then paste your URL + anon key into it
npm install
npm run dev                     # open http://localhost:3000
```
Supabase → **Authentication → URL Configuration → Redirect URLs**: add `http://localhost:3000/**`.

### 4. Deploy on Vercel
1. Push this folder to GitHub (inside the Pomogochi repo as `webapp/`, or as its own repo).
2. vercel.com → **Add New → Project** → import the repo.
   - If it's inside Pomogochi: set **Root Directory = `webapp`**.
3. **Environment Variables**: `NEXT_PUBLIC_SUPABASE_URL` and `NEXT_PUBLIC_SUPABASE_ANON_KEY` → **Deploy**.
4. Supabase → **Authentication → URL Configuration**:
   - **Site URL** = your Vercel URL (for example `https://pomogochi.vercel.app`)
   - **Redirect URLs**: add `https://pomogochi.vercel.app/**`

Done: open the Vercel URL, sign in, pick a pet, and upload a day.

## Files

```
webapp/
├── app/
│   ├── page.tsx              landing + Google sign-in
│   ├── auth/callback/        finishes the Google login
│   ├── onboarding/           pick pet + name
│   ├── home/                 pet view + stats
│   ├── upload/               drag & drop day sheets
│   ├── history/              calendar + day list
│   ├── settings/             switch pet, sign out, start over
│   ├── actions.ts            server actions (save pet, upload, delete)
│   └── globals.css
├── components/               Nav, PetView, PetPicker, Uploader, SignIn
├── lib/
│   ├── sheet.ts              day sheet parser + validation + growth rule
│   ├── pets.ts               pets, stages, art
│   ├── data.ts               loads user, pet, days
│   └── supabase/             browser + server clients
├── middleware.ts             keeps you signed in, protects pages
├── supabase/schema.sql       tables + row-level security
└── public/pets/              Fluent Emoji 3D art + MIT license
```

## Credits
- Pet art: [Microsoft Fluent Emoji](https://github.com/microsoft/fluentui-emoji), MIT (see `public/pets/LICENSE-fluent-emoji.txt`).
- Made by **Teja Manepalli** 🇮🇳
