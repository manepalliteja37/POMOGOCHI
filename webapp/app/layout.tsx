import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Pomogochi · grow a pet with your focus",
  description: "Upload your Pomodoro Rabbit day sheets and grow a dog, rabbit, parrot or plant.",
  icons: { icon: "/pets/rabbit_face_3d.png" },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
