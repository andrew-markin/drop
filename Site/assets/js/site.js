// Highlights the download for the visitor's system and puts it first
document.addEventListener("DOMContentLoaded", () => {
  const agent = navigator.userAgent;
  let os = null;
  if (/Windows/.test(agent)) {
    os = "windows";
  } else if (/Fedora|Red Hat|CentOS|AlmaLinux|Rocky/i.test(agent)) {
    os = "rpm";
  } else if (/Linux|X11/.test(agent) && !/Android/.test(agent)) {
    os = "deb";
  }
  const card = os && document.querySelector(`.download[data-os="${os}"]`);
  if (card) {
    card.classList.add("recommended");
    card.querySelector(".button").classList.add("primary");
    card.parentElement.prepend(card);
  }

  // The language list closes on a click outside of it or on Escape, like a regular menu
  const languages = document.querySelector(".lang");
  document.addEventListener("click", (event) => {
    if (languages.open && !languages.contains(event.target)) {
      languages.open = false;
    }
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && languages.open) {
      languages.open = false;
      languages.querySelector("summary").focus();
    }
  });
});
