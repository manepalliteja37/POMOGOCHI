# Pomogochi Companion: Launch Guide (no coding needed)

About 30 minutes. You need a Google account, a GitHub account and a computer. Everything is free.

## Step 0: Open a notepad
Make a "Pomogochi keys" note. You'll copy 5 things into it as you go:
1. Supabase Project URL
2. Supabase anon key
3. Supabase callback URL
4. Google Client ID
5. Google Client secret
(Also save the Supabase database password.) Keep this note private.

## Step 1: Create the database (Supabase)
1. Go to supabase.com → Start your project → Continue with GitHub.
2. New project → name `pomogochi`, generate a password (save it), region South Asia (Mumbai) → Create. Wait about 2 minutes.
3. Left menu SQL Editor → New query. Open `webapp/supabase/schema.sql` in Notepad, select all, copy, paste → Run. You should see "Success. No rows returned".
4. Project Settings (gear) → API / API Keys → copy the Project URL (#1) and the anon public key (#2). It may sit under "Legacy API keys". Never copy service_role.
5. Authentication → Sign In / Providers → Google → copy the Callback URL (#3). Leave the page open.

## Step 2: Turn on Google sign-in (Google Cloud)
1. console.cloud.google.com → project dropdown → New Project → `Pomogochi` → Create → select it.
2. Search "OAuth consent screen" (or Google Auth Platform) → Get started → app name Pomogochi, your email, Audience External → Create.
3. Audience → Test users → + Add users → your Gmail (and any friends) → Save. Don't publish yet: Google needs a homepage + privacy policy URL first. Publish after Vercel is live (Branding → add the links → Audience → Publish app).
4. Clients (or Credentials → Create credentials → OAuth client ID) → Web application → name Pomogochi web.
5. Authorized redirect URIs → Add URI → paste #3 → Create.
6. Copy the Client ID (#4) and Client secret (#5).
7. Back in Supabase Google page: toggle Enable, paste #4 and #5 → Save.

## Step 3: Put the code on GitHub
1. Unzip so you have `Pomogochi/webapp/package.json`.
2. Right-click the Pomogochi folder → Open Git Bash here.
3. Run:
   git add webapp
   git commit -m "Add companion web app"
   git push
4. Check github.com/<you>/Pomogochi shows a webapp folder.

## Step 4: Put it online (Vercel)
1. vercel.com → Sign Up → Continue with GitHub → Hobby.
2. Add New → Project → Import Pomogochi (Adjust GitHub permissions if missing).
3. Root Directory → Edit → pick webapp → Continue.
4. Environment Variables:
   NEXT_PUBLIC_SUPABASE_URL = #1
   NEXT_PUBLIC_SUPABASE_ANON_KEY = #2
5. Deploy → wait for Congratulations → copy your link (https://something.vercel.app).

## Step 5: Tell Supabase your link
Supabase → Authentication → URL Configuration:
- Site URL = your Vercel link → Save.
- Redirect URLs → Add URL → your link + `/**` → Save.

## Step 6: Try it
Open your link → Sign in with Google → pick a pet → upload a day sheet.

## If something goes wrong
- Error 400 redirect_uri_mismatch → the URI in Google step 2.5 doesn't exactly match #3.
- After login you land on localhost → Step 5 Site URL not saved.
- "Sign-in didn't finish" → Step 5 Redirect URL missing /**.
- "This app isn't verified" → click Advanced → Go to Pomogochi (or Publish app in 2.3).
- Blank page / Invalid API key → fix the env vars in Vercel → Deployments → ⋯ → Redeploy.
- Vercel build failed → Root Directory isn't webapp.
