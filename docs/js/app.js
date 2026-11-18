/**
 * Helper UI per la pagina GitHub Pages ESP32-S3 Web Flasher.
 */
document.addEventListener("DOMContentLoaded", () => {
  const serialBadge = document.getElementById("serial-badge");

  if (serialBadge) {
    if ("serial" in navigator) {
      serialBadge.textContent = "Web Serial Supportata";
      serialBadge.classList.add("badge-ok");
    } else {
      serialBadge.textContent = "Web Serial Non Supportata";
      serialBadge.classList.add("badge-warn");
    }
  }

  document.querySelectorAll(".copy-btn").forEach((button) => {
    button.addEventListener("click", async () => {
      const value = button.getAttribute("data-copy");
      if (!value) return;

      const originalLabel = button.textContent;
      try {
        await navigator.clipboard.writeText(value);
        button.textContent = "Copiato!";
      } catch {
        button.textContent = "Errore";
      }

      setTimeout(() => {
        button.textContent = originalLabel;
      }, 1400);
    });
  });
});
