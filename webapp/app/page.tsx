import { redirect } from "next/navigation";
import { createClient } from "@/lib/supabase/server";
import SignIn from "@/components/SignIn";

export default async function Landing({ searchParams }: { searchParams: Promise<{ error?: string }> }) {
  const supabase = await createClient();
  const { data: { user } } = await supabase.auth.getUser();
  if (user) redirect("/home");
  const { error } = await searchParams;

  return (
    <main className="landing">
      <div className="row4">
        <img src="/pets/dog_3d.png" alt="" className="pet breathe" style={{ animationDelay: "0s" }} />
        <img src="/pets/rabbit_3d.png" alt="" className="pet breathe" style={{ animationDelay: ".4s" }} />
        <img src="/pets/parrot_3d.png" alt="" className="pet breathe" style={{ animationDelay: ".8s" }} />
        <img src="/pets/sunflower_3d.png" alt="" className="pet breathe" style={{ animationDelay: "1.2s" }} />
      </div>
      <h1>Pomogochi</h1>
      <p className="lead">
        Your <b>Pomodoro Rabbit</b> tracks every focus. Upload its day sheets here and grow a
        dog, rabbit, parrot or plant from your real work.
      </p>
      <SignIn />
      {error && <p className="err">Sign-in didn&apos;t finish. Please try again.</p>}
      <div className="how">
        <div><b>1</b> Focus on the device</div>
        <div><b>2</b> Web page → History → JSON</div>
        <div><b>3</b> Upload it here</div>
        <div><b>4</b> Watch your pet grow</div>
      </div>
    </main>
  );
}
