# Lyspote – opsætning af Shopify

Butikken sælger refleksudstyr til hunde til danske hundeejere. Følg trinnene i rækkefølge.
Se forhåndsvisningen af butikken (`website/butik/index.html`) for at se, hvordan siderne skal se ud.

> **Inden du betaler for noget:** Tjek at navnet "Lyspote" er ledigt. Søg i varemærkeregistrene på
> dkpto.dk og euipo.europa.eu (TMview), og tjek at domænet `lyspote.dk` er ledigt på punktum.dk.
> Er navnet taget, så vælg et andet. Navnet bruges kun i teksterne og er nemt at skifte.

---

## Produkterne

Alle produkter passer til samme kunde: en hundeejer, der går tur i mørke og regn. Det gør det nemt at sælge
flere ting til hver kunde, og produkterne kan sælges sammen som et sæt.

| Produkt | Salgspris | Anslået indkøb* | Bruttomargin | Vægt |
|---|---|---|---|---|
| Refleks-sele – trækfri (S–XL) | 399 kr. | ca. 60 kr. | ca. 81 % | 200 g |
| Regnjakke med refleks (S–XL) | 449 kr. | ca. 70 kr. | ca. 81 % | 300 g |
| Refleks-snor 1,5 m (sort/orange) | 249 kr. | ca. 35 kr. | ca. 82 % | 150 g |
| Vandtæt bilsædebetræk | 549 kr. | ca. 120 kr. | ca. 73 % | 1.200 g |
| Snusemåtte | 299 kr. | ca. 50 kr. | ca. 79 % | 400 g |
| **Mørke-gåtur-sættet** (sele + snor + jakke) | **899 kr.** | ca. 165 kr. | ca. 77 % | 650 g |

\* Indkøb inkl. fragt og told til Danmark, anslået ud fra Alibaba-priser (sele ca. 1–7 $, regnjakke ca. 1–8 $ per stk.
ved 100+ stk.). For snusemåtte og bilsædebetræk fandt jeg ingen priser, så de tal er gæt. **Ret alle tal, når du har
fået rigtige tilbud fra leverandørerne.** Bruttomargin = (salgspris uden moms − indkøb) / salgspris uden moms.

**Hvorfor de her produkter:**
- Der er ca. 800.000–850.000 hunde i Danmark (Dansk Hunderegister), og danske hunde skal luftes i mørke fra oktober til marts.
- Ingen af produkterne kræver særlige godkendelser. Der er ingen batterier, ingen elektronik og ingen fødevarer.
- De er lette og små, så de kan sendes billigt til en pakkeshop.
- Sættet til 899 kr. betyder, at en kunde fra en annonce har råd til at koste 300 kr. eller mere.

**Start småt:** Bestil sele, snor og regnjakke først (dem der indgår i sættet). Vent med bilsædebetræk og
snusemåtte, til de første varer sælger.

**Søgeord på Alibaba:**
- `reflective no pull dog harness custom logo`
- `waterproof reflective dog raincoat harness hole`
- `reflective dog leash padded handle custom logo`
- `waterproof dog car back seat cover`
- `snuffle mat dog`

**Besked til leverandører** (kopiér og ret til):

> Hello, I run a pet brand in Denmark. I'm interested in your [product]. Could you please tell me:
> 1) price per unit for 100, 300 and 500 pcs, 2) MOQ with my logo on the product and on the packaging,
> 3) sample cost and shipping to Denmark by express, 4) production time, 5) materials and whether you have
> test reports (e.g. REACH). I would like sizes S/M/L/XL and need the size chart in cm. Thank you!

**Timing:** Det er november. Med samples (1–2 uger) og produktion og luftfragt (3–5 uger) er
varerne her i slutningen af december eller januar. Mørket varer til marts, så du har hele januar og februar
til at sælge. Bestil allerede nu, så du er klar til næste efterår (september) med det samme.

---

## 1. Opret Shopify (dag 1)

1. Opret en konto på shopify.com og vælg den billigste plan ("Basic"), når prøveperioden slutter.
2. **Indstillinger → Butiksdetaljer:** Butiksnavn `Lyspote`, land Danmark, valuta DKK, tidszone København.
   Indtast dit firmanavn, adresse og CVR.
3. **Indstillinger → Sprog:** Sæt dansk som standardsprog.
4. **Indstillinger → Skatter og afgifter:** Danmark, 25 %. Slå til: "Alle priser er inklusive moms".
5. **Indstillinger → Domæner:** Køb eller forbind dit domæne (fx `lyspote.dk`).

## 2. Importér produkterne

1. **Produkter → Importér** og vælg `website/shopify/produkter.csv`.
2. Shopify viser en forhåndsvisning. Hvis der står fejl i en kolonne, så skriv til mig, så retter jeg filen.
3. Produkterne importeres som **kladder** (status "draft") med lager 0. Det er med vilje.
4. Gå ind på hvert produkt og:
   - Upload 6–8 billeder (dine egne fotos af varerne, ikke leverandørens).
   - Udfyld leverandør og importør i afsnittet "Produktsikkerhed" i beskrivelsen. Det er lovkrav (GPSR).
   - Sæt lagerantallet, når varerne er kommet.
   - Skift status til "Aktiv".
5. **Sættet:** Installér den gratis app **Shopify Bundles**, og lav "Mørke-gåtur-sættet" som et bundle af
   sele + snor + jakke, så lageret trækkes rigtigt. Slet så det importerede sæt-produkt, eller brug det som
   skabelon for tekst og billeder.
6. **Produkter → Kollektioner:** Lav kollektionen "Alle produkter" og "Til mørket" (tag: refleks).

## 3. Tema og design

1. **Onlinebutik → Temaer:** Brug gratis-temaet **Dawn** (eller "Refresh").
2. **Tilpas → Temaindstillinger → Farver:**
   - Baggrund `#0E1A2B` · Sekundær baggrund `#15243A` · Tekst `#EEF1F5`
   - Knapper og accent `#F4B740` · Knaptekst `#1A1305`
3. **Typografi:** Overskrifter "Bricolage Grotesque" eller en anden kraftig sans serif. Brødtekst "Inter".
4. **Byg forsiden med disse sektioner, i denne rækkefølge** (de passer 1:1 til forhåndsvisningen):

| Sektion i Dawn | Indhold |
|---|---|
| Meddelelsesbjælke | Fri fragt over 499 kr. · Sendes fra Danmark · 1–2 hverdage · MobilePay |
| Billedbanner | Overskrift: "Gåture i mørket. Uden at være usynlig." Tekst: "Seler, snore og regnjakker med refleks hele vejen rundt. Designet til danske vintre, sendt fra Danmark." Knap: "Se Mørke-gåtur-sættet" |
| Multikolonne (4) | Levering på 1–2 dage · Betal med MobilePay · 30 dages returret · Dansk kundeservice |
| Fremhævet produkt | Mørke-gåtur-sættet |
| Fremhævet kollektion | Alle produkter |
| Multikolonne (3) | "Synlig fra alle sider" · "Tørt og varmt" · "Fra Danmark, ikke fra Kina" (teksterne står i forhåndsvisningen) |
| Kollapsible indhold | De 5 spørgsmål fra FAQ |
| E-mailtilmelding | "Få 10 % rabat på din første ordre" |

5. Logoet er en pote i ravgul (`#F4B740`) og ordet LYSPOTE. Lav det gratis i Canva.

## 4. Betaling

1. **Indstillinger → Betalinger:** Aktivér **Shopify Payments** (kort, Apple Pay, Google Pay).
2. Tilføj **MobilePay**. Det kan være slået til i Shopify Payments, ellers skal du bruge en betalingsudbyder som
   QuickPay eller Shipmondo Payments. Uden MobilePay mister du mange danske kunder.

## 5. Fragt

1. Installér appen **Shipmondo** og forbind GLS og/eller PostNord.
2. **Indstillinger → Forsendelse:**
   - Pakkeshop: 39 kr.
   - Levering til døren: 59 kr.
   - Gratis fragt ved køb over 499 kr.
3. Slå pakkeshop-valg til i kassen via Shipmondo.

## 6. Lovpligtige sider

**Indstillinger → Politikker:** Shopify kan lave udkast til returpolitik, privatlivspolitik og
servicevilkår. Ret dem til danske regler:

- **Fortrydelsesret:** Mindst 14 dage efter modtagelse. Vi tilbyder 30 dage og gratis bytte af størrelse.
- Skriv, hvem der betaler returfragten ved fortrydelse (typisk kunden), og at der skal bruges en standardformular.
- Firmanavn, adresse, CVR, e-mail og telefon skal stå i bunden af siden og i handelsbetingelserne.
- Cookie-banner: installér fx Consentmo eller Cookie Information (gratis eller billig plan).

Lav også disse sider under **Onlinebutik → Sider:** *Om os*, *Levering og fragt*, *Returnering og bytte*,
*Størrelsesguide*, *Kontakt*.

**Om os – udkast:**

> Lyspote startede på en mørk novemberaften, hvor en bil først så min hund i sidste øjeblik.
> Vi laver udstyr, der gør gåturen i mørke og regn tryg, både for hunden og for dig. Vi har alt på lager i Danmark,
> så du har det i hænderne på 1–2 dage. Har du spørgsmål, så skriv til os. Det er os selv, der svarer.

(Skriv din egen, sande historie. Den skal ikke være opdigtet.)

## 7. Apps (hold det til disse)

| App | Formål | Pris |
|---|---|---|
| Shipmondo | Fragt og labels | Betal per label |
| Klaviyo | E-mail og automatiske flows | Gratis op til et vist antal kontakter |
| Judge.me | Produktanmeldelser | Gratis plan |
| Trustpilot | Butiksanmeldelser | Gratis plan |
| Shopify Bundles | Mørke-gåtur-sættet | Gratis |
| Dinero eller Billy | Regnskab | Fra 0 kr. |
| Meta (Facebook & Instagram) | Pixel og Conversions API | Gratis |
| Google & YouTube | Google Shopping | Gratis |
| Consentmo | Cookie-samtykke | Gratis eller billig plan |

## 8. E-mails i Klaviyo

**Velkomst (efter tilmelding)**
1. Straks: "Her er din rabatkode: 10 % på din første ordre" + Mørke-gåtur-sættet.
2. Dag 2: "3 tips til sikre gåture i mørket" (nyttigt indhold, ingen hård salgstale).
3. Dag 4: "Din rabatkode udløber snart".

**Forladt kurv**
1. Efter 1 time: "Glemte du noget?" med billede af varen.
2. Efter 24 timer: "Fri fragt over 499 kr. – og 30 dages returret".

**Efter køb**
1. Når pakken er sendt: "Din pakke er på vej" + størrelsesguide.
2. Dag 7: "Hvordan passer den?" + bed om en anmeldelse.
3. Dag 30: tilbud på det produkt, kunden ikke har købt (fx snusemåtte eller bilsædebetræk).

## 9. Test før du åbner

- [ ] Læg en vare i kurven og betal med MobilePay og med kort (brug Shopifys testtilstand eller refundér bagefter).
- [ ] Tjek at du får ordremailen, og at Shipmondo laver en label.
- [ ] Åbn butikken på din mobil. Næsten halvdelen af danskernes onlinekøb sker på mobilen.
- [ ] Tjek at CVR, adresse og alle politiksider er synlige.
- [ ] Bed 2–3 venner om at købe og give dig ærlig feedback.

## 10. Sådan går du i gang med salget

Se `website/index.html` (Ecom-modellen), del 7 og del 9. Kort fortalt:

1. Lav 15–20 korte videoer med telefonen: hunden i mørke med og uden refleks i billygter. Kontrasten sælger produktet.
2. Find 3–5 danske hunde-profiler på Instagram og TikTok og send dem et gratis sæt for en ærlig video.
3. Start Meta-annoncer i Danmark med 150–300 kr. om dagen. Sluk de annoncer, der koster mere per salg end din
   Max CPA (find den i beregneren i Ecom-modellen).
4. Spørg om lov, før du deler butikken i danske hundegrupper på Facebook.

> Det her er ikke juridisk rådgivning. Tal med Erhvervshus eller en revisor om moms, import og
> handelsbetingelser, før du sælger.
