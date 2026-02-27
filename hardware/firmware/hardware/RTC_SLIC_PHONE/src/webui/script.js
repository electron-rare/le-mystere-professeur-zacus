let contactsData = [];

function showSection(section) {
  const map = {
    contacts: "contactsSection",
    config: "configSection",
    logs: "logsSection",
    control: "controlSection",
  };
  Object.values(map).forEach((id) => {
    const el = document.getElementById(id);
    if (el) {
      el.classList.remove("active");
    }
  });
  const sectionEl = document.getElementById(map[section]);
  if (sectionEl) {
    sectionEl.classList.add("active");
  }
}

async function safeFetchJson(url, options = {}) {
  const response = await fetch(url, options);
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

async function refreshStatus() {
  const status = document.getElementById("status");
  try {
    const data = await safeFetchJson("/api/status");
    status.textContent =
      `state=${data.state} board=${data.board_profile || "n/a"} ` +
      `telephony=${data.telephony || "n/a"} hook=${data.hook || "n/a"} ` +
      `full_duplex=${data.full_duplex} underrun=${data.audio_underrun_count || 0} ` +
      `drop=${data.audio_drop_frames || 0}`;
  } catch (error) {
    status.textContent = `Erreur statut: ${error.message}`;
  }
}

async function loadContacts() {
  try {
    contactsData = await safeFetchJson("/api/contacts");
    renderContacts();
  } catch (error) {
    document.getElementById("contactFeedback").textContent = `Erreur contacts: ${error.message}`;
  }
}

function renderContacts() {
  const list = document.getElementById("contactsList");
  const searchInput = document.getElementById("searchContact");
  const search = (searchInput?.value || "").toLowerCase();
  list.innerHTML = "";

  contactsData
    .filter((c) => c.nom.toLowerCase().includes(search) || c.numero.includes(search))
    .forEach((c, idx) => {
      const card = document.createElement("div");
      card.className = "contact-card";
      card.innerHTML = `<b>${c.nom}</b><br><span>${c.numero}</span><br><span>${c.type}</span>`;

      const actions = document.createElement("div");
      actions.className = "contact-actions";
      actions.innerHTML =
        `<button data-call="${c.numero}">Appeler</button>` +
        `<button data-edit="${idx}">Modifier</button>` +
        `<button data-delete="${idx}">Supprimer</button>`;
      card.appendChild(actions);
      list.appendChild(card);
    });
}

function editContact(idx) {
  const c = contactsData[idx];
  if (!c) {
    return;
  }
  const form = document.getElementById("contactForm");
  form.nom.value = c.nom;
  form.numero.value = c.numero;
  form.type.value = c.type;
  form.dataset.editIdx = String(idx);
}

async function deleteContact(idx) {
  const response = await fetch("/api/contacts", {
    method: "DELETE",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ idx }),
  });
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  await loadContacts();
  document.getElementById("contactFeedback").textContent = "Contact supprimé";
}

async function callContact(numero) {
  await sendControl("call", { numero });
  document.getElementById("contactFeedback").textContent = `Appel lancé vers ${numero}`;
}

async function loadConfig() {
  try {
    const data = await safeFetchJson("/api/config");
    document.getElementById("config").textContent = JSON.stringify(data, null, 2);
  } catch (error) {
    document.getElementById("config").textContent = `Erreur config: ${error.message}`;
  }
}

async function saveConfig(event) {
  event.preventDefault();
  const form = event.target;
  const payload = {
    param1: form.param1.value || "valeur1",
    param2: form.param2.value || "valeur2",
  };
  const response = await fetch("/api/config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  if (!response.ok) {
    document.getElementById("config").textContent = `Erreur config: HTTP ${response.status}`;
    return;
  }
  await loadConfig();
}

async function refreshLogs() {
  const response = await fetch("/api/logs");
  const logs = response.ok ? await response.text() : `Erreur logs: HTTP ${response.status}`;
  document.getElementById("logs").textContent = logs;
}

async function sendControl(action, extraPayload = {}) {
  const response = await fetch("/api/control", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ action, ...extraPayload }),
  });
  const body = await response.text();
  document.getElementById("controlResult").textContent = body;
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return true;
}

function bindEvents() {
  document.querySelectorAll("nav button[data-section]").forEach((button) => {
    button.addEventListener("click", () => showSection(button.dataset.section));
  });

  document.getElementById("refreshStatusBtn").addEventListener("click", refreshStatus);
  document.getElementById("refreshLogsBtn").addEventListener("click", refreshLogs);
  document.getElementById("searchContact").addEventListener("input", renderContacts);
  document.getElementById("configForm").addEventListener("submit", saveConfig);

  document.getElementById("contactForm").addEventListener("submit", async (event) => {
    event.preventDefault();
    const form = event.target;
    const editIdxRaw = form.dataset.editIdx;
    const hasEditIdx = typeof editIdxRaw !== "undefined";
    const payload = {
      nom: form.nom.value,
      numero: form.numero.value,
      type: form.type.value,
    };
    const method = hasEditIdx ? "PUT" : "POST";
    const body = hasEditIdx ? { ...payload, idx: Number(editIdxRaw) } : payload;

    const response = await fetch("/api/contacts", {
      method,
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (!response.ok) {
      document.getElementById("contactFeedback").textContent = `Erreur contact: HTTP ${response.status}`;
      return;
    }
    delete form.dataset.editIdx;
    form.reset();
    document.getElementById("contactFeedback").textContent = hasEditIdx
      ? "Contact modifié"
      : "Contact ajouté";
    await loadContacts();
  });

  document.getElementById("contactsList").addEventListener("click", async (event) => {
    const target = event.target;
    if (!(target instanceof HTMLElement)) {
      return;
    }
    try {
      if (target.dataset.call) {
        await callContact(target.dataset.call);
      }
      if (target.dataset.edit) {
        editContact(Number(target.dataset.edit));
      }
      if (target.dataset.delete) {
        await deleteContact(Number(target.dataset.delete));
      }
    } catch (error) {
      document.getElementById("contactFeedback").textContent = error.message;
    }
  });

  document.querySelectorAll("#controlSection button[data-action]").forEach((button) => {
    button.addEventListener("click", async () => {
      try {
        await sendControl(button.dataset.action);
      } catch (error) {
        document.getElementById("controlResult").textContent = error.message;
      }
    });
  });
}

document.addEventListener("DOMContentLoaded", async () => {
  bindEvents();
  await Promise.all([refreshStatus(), loadContacts(), loadConfig(), refreshLogs()]);
  showSection("contacts");
});
