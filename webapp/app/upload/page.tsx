import Nav from "@/components/Nav";
import Uploader from "@/components/Uploader";
import { loadMe } from "@/lib/data";

export default async function UploadPage() {
  await loadMe();
  return (
    <>
      <Nav active="upload" />
      <main className="page narrow">
        <h1>Upload day sheets</h1>
        <p className="lead">
          On the device web page (<code>pomodoro.local</code>), open <b>History</b> and tap <b>JSON</b> next to a day.
          Drop one or many files here. Uploading the same day again replaces it, so nothing counts twice.
        </p>
        <Uploader />
      </main>
    </>
  );
}
