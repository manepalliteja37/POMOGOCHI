"use client";
import { createClient } from "@/lib/supabase/client";

export default function SignIn() {
  async function go() {
    const supabase = createClient();
    await supabase.auth.signInWithOAuth({
      provider: "google",
      options: { redirectTo: `${window.location.origin}/auth/callback` },
    });
  }
  return (
    <button className="btn primary big" onClick={go}>
      <span className="g">G</span> Sign in with Google
    </button>
  );
}
