# Code Flow & Architectural Smells Analyzer (Konsept va Arxitektura)

Ushbu hujjat o'rta va yirik kod bazalarida an'anaviy **linter** va **unit testlar** ilg'ay olmaydigan ko'rinmas muammolarni (arxitektura chalkashliklari, yashirin mantiqiy dublikatlar, o'lik/ortiqcha o'tish qatlamlari, "God Object"lar) aniqlash va ularni vizuallashtirish algoritmlari bo'yicha tizimli qo'llanma hisoblanadi.

---

## 1. Muammo Ta'rifi: Nega Linter va Testlar Yetarli Emas?

* **Linterlar (ESLint, Flake8, golangci-lint):** Faqat lokal, bitta fayl yoki funksiya doirasidagi sintaktik qoidalarni (uslub, ishlatilmagan o'zgaruvchi, format) tekshiradi. Ular global tizim strukturasini ko'ra olmaydi.
* **Testlar:** Kodning kutilgan natijani berishini (`A kirsa, B chiqishi kerak`) tekshiradi. Ammo kodning *qanchalik chalkash, ortiqcha murakkab yoki nozik* ekanligi haqida ma'lumot bermaydi. Testlar "yashil" bo'lsa ham arxitektura "chirigan" (architectural erosion) bo'lishi mumkin.
* **Asosiy maqsad:** Kod oqimi qayerda qanday tarmoqlanganini, qaysi modullar bir-birini asossiz ravishda chigallashtirganini va qaysi mantiq qayta-qayta yozilganini vizual tahlil qilish.

---

## 2. Tizimning Matematik Modeli (Graf Qatlamlari)

Kod bazasini matn emas, ko'p qatlamli **Yo'naltirilgan Graf (Directed Graph)** sifatida modellashtirish zarur:

```
[ AST (Sintaktik daraxt) ]
         │
         ▼
[ Control Flow Graph (CFG) ] ──► Funksiya ichidagi ijro yo'llari
         │
         ▼
[ Call Graph (Chaqiruvlar grafi) ] ──► Funksiyalararo bog'liqliklar
         │
         ▼
[ Dependency Graph (Modul grafi) ] ──► Modul/Paket darajasidagi arxitektura
```

1. **AST (Abstract Syntax Tree):** Har bir fayldagi strukturani (sinflar, funksiyalar, operatorlar) daraxt shakliga keltiradi.
2. **Call Graph:** Qaysi funksiya qaysi birini chaqirishini (`fnA() -> fnB() -> fnC()`) ko'rsatadi.
3. **Dependency Graph:** Fayllar va modullar o'rtasidagi import/export bog'liqliklarini ko'rsatadi.

---

## 3. "Ko'rinmas" Muammolarni Topish Algoritmlari

### A. Yashirin va Mantiqiy Dublikatlar (Type-3 va Type-4 Clones)
* **Muammo:** Ikki xil dasturchi turli modullarda bir xil biznes-mantiqni (masalan, valyuta hisoblash, to'lov tekshiruvi) boshqacha o'zgaruvchi nomlari yoki boshqacha sikllar bilan yozgan.
* **Algoritm:**
  * Funksiyalarning AST daraxtidan mantiqiy sub-daraxtlar olinadi (**AST Subtree Normalization**).
  * Funksiyalarning semantik ma'nosi embedding vektorlarga aylantiriladi (**Code2Vec / Graph Neural Networks**).
  * **Kosinus o'xshashligi (Cosine Similarity):** Agar turli joydagi funksiyalarning o'xshashligi 85%+ bo'lsa, tizim buni "yashirin dublikat" deb belgilaydi.

### B. Aylanma Bog'liqliklar (Circular Dependencies & Spaghetti Architecture)
* **Muammo:** `Modul A -> Modul B -> Modul C -> Modul A`. Bu loyihani bo'laklash, sinovdan o'tkazish va refaktoring qilishni deyarli imkonsiz qiladi.
* **Algoritm:**
  * **Tarjan / Kosaraju algoritmi (Strongly Connected Components - SCC):** Yo'naltirilgan grafdagi barcha aylanma zanjirlarni $O(V + E)$ vaqtda topadi.
  * **Robert C. Martin Metriklari:**
    * Afferent Coupling ($C_a$) — Modulga tashqaridan nechta bog'liqlik kiryapti?
    * Efferent Coupling ($C_e$) — Modul tashqariga nechta modulga tayanyapti?
    * Beqarorlik darajasi: $I = \frac{C_e}{C_a + C_e}$. Agar beqaror modul butun tizim poydevoriga aylangan bo'lsa, bu arxitektura buzilishidir.

### C. "God Object" (Xudo-modul) va "Shotgun Surgery" Xavfi
* **Muammo:** Bitta modul/klass juda ko'p narsani biladi va butun tizim unga qaram bo'lib qolgan. Bitta o'zgarish 15 ta faylga ta'sir qiladi.
* **Algoritm:**
  * **Betweenness Centrality & PageRank:** Grafdagi barcha eng qisqa yo'llar qaysi tugun orqali o'tishini hisoblash. Agar biror modul/funksiya ko'rsatkichi me'yordan keskin yuqori bo'lsa, u haddan tashqari ko'p mas'uliyatni o'ziga olgan.

### D. Foydasiz Tranzit Qatlamlar (Pass-Through Wrappers / Layering Hell)
* **Muammo:** Testlar o'tadi, lekin funksiya shunchaki boshqasini chaqiradi va hech qanday biznes-qiymat qo'shmaydi (ortiqcha abstruktsiya botqog'i).
* **Algoritm:**
  * **Program Slicing & Information Flow Analysis:** Agar tugun kiruvchi argumentlarni o'zgartirmasdan, hech qanday holat (state) yoki nojo'ya ta'sir (side-effect) yaratmasdan keyingi tugunga uzatsa, bu zanjir "ortiqcha overhead" deb belgilanadi.

---

## 4. Vizuallashtirish Usullari

Chalkashlikni tushunish uchun to'g'ri vizual metafora kerak:

| Vizualizatsiya Turi | Qanday Ko'rinadi? | Nima Muammoni Ko'rsatadi? |
| :--- | :--- | :--- |
| **Dependency Structure Matrix (DSM)** | Fayllar koordinata o'qlarida joylashadi, kesishmalar belgilanadi. | Agar belgilar bosh diagonaldan uzoqlashsa, arxitektura qatlamlari buzilgan. |
| **Force-Directed Graph** | Klasterlar tortishish kuchi orqali joylashadi. | Toza kod orolchalarga ajraladi; chalkash kod esa chigal "yung to'pi" (hairball) hosil qiladi. |
| **Software City (CodeCity)** | Kod bazasi shahar kabi 3D xaritada aks ettiriladi. | Bino balandligi = LOC (hajm); Rangi = Murakkablik (qizil = xavfli zona). |

---

## 5. Tizim Arxitekturasi va Qurish Bosqichlari (MVP Roadmap)

```
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│  Source Code    │ ────► │  Parser Engine  │ ────► │ Graph Database  │
│  (Go, TS, Py)   │       │  (Tree-sitter)  │       │ (NetworkX/Neo4j)│
└─────────────────┘       └─────────────────┘       └────────┬────────┘
                                                             │
                                                             ▼
┌─────────────────┐       ┌─────────────────┐       ┌─────────────────┐
│ Visual Frontend │ ◄──── │ Smells Engine   │ ◄──── │ Graph Analytics │
│ (D3.js/Cytoscape│       │ (Cycles, Clones)│       │ (PageRank, SCC) │
└─────────────────┘       └─────────────────┘       └─────────────────┘
```

1. **Parser bosqichi:**
   * Universal vosita: `Tree-sitter` (har qanday til uchun tezkor AST tuzib beradi).
2. **Graf shakllantirish:**
   * Dastlab Python (`NetworkX`) yoki Graph Database (`Neo4j`) da tugun (fayl/funksiya) va qirralarni (import/chaqiruv) yig'ish.
3. **Analitik yadro:**
   * Sikllarni aniqlash (`Tarjan`).
   * "God modules"ni aniqlash (`PageRank` va `Degree Centrality`).
   * Dublikatlarni tekshirish (AST subtree hashing / semantic vectors).
4. **Vizuallashtirish:**
   * Frontend: `D3.js`, `Cytoscape.js` yoki `3d-force-graph` orqali brauzerda interaktiv ko'rsatish.

---

## 6. Real Hayotdagi Qiyinchiliklar

1. **Dinamik chaqiruvlar:** Python yoki JavaScript'dagi dinamik importlar (`require(varName)`) yoki reflection orqali chaqirishlar statik tahlilda qiyin ushlanadi (Statik tillar — Go, Rust, Java, TypeScript bilan ishlash ancha aniq natija beradi).
2. **Katta hajmdagi graflar (Graph explosion):** 500,000+ qatorli loyihada millionlab qirralar hosil bo'ladi. Tahlilni bosqichma-bosqich ierarxiya bo'yicha yurgizish shart: `Repo ──► Modullar ──► Fayllar ──► Funksiyalar`.
