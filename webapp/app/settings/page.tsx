import Nav from "@/components/Nav";
import PetPicker from "@/components/PetPicker";
import { loadMe } from "@/lib/data";
import { resetEverything, signOut } from "@/app/actions";

export default async function Settings() {
  const { user, pet } = await loadMe();
  return (
    <>
      <Nav active="settings" />
      <main className="page narrow">
        <h1>Your pet</h1>
        <PetPicker initialKind={pet!.kind} initialName={pet!.name} />

        <div className="card account">
          <p>Signed in as <b>{user.email}</b></p>
          <form action={signOut}><button className="btn ghost">Sign out</button></form>
          <details>
            <summary>Start over</summary>
            <p className="hint">Deletes your pet and every uploaded day. This can&apos;t be undone.</p>
            <form action={resetEverything}><button className="btn danger">Delete everything</button></form>
          </details>
        </div>
        <p className="hint center">
          Pet art: <a href="https://github.com/microsoft/fluentui-emoji">Microsoft Fluent Emoji</a> (MIT)
        </p>
      </main>
    </>
  );
}
