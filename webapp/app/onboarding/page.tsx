import { loadMe } from "@/lib/data";
import PetPicker from "@/components/PetPicker";

export default async function Onboarding() {
  const { pet } = await loadMe({ needPet: false });
  return (
    <main className="page narrow">
      <h1>{pet ? "Change your pet" : "Choose your pet"}</h1>
      <p className="lead">It grows from the focus sessions and tasks you finish on your Pomodoro Rabbit.</p>
      <PetPicker initialKind={pet?.kind ?? null} initialName={pet?.name ?? ""} />
    </main>
  );
}
