// Validate programs with the OFFICIAL sandbox page (headless Chromium).
// usage: node sandbox_check.js INPUT OUTPUT [caseIdx ...]
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
(async () => {
  const [inp, out, ...idxArg] = process.argv.slice(2);
  const L = fs.readFileSync(inp, 'utf8').split('\n');
  const n = parseInt(L[0]); let i = 1; const cases = [];
  for (let k = 0; k < n; k++) {
    while (!L[i].trim()) i++;
    const rows = parseInt(L[i].split(' ')[1]);
    cases.push(L.slice(i, i + 1 + rows).join('\n')); i += 1 + rows;
  }
  const progs = fs.readFileSync(out, 'utf8').replace(/\s+$/, '').split(/\n\s*\n/);
  const html = path.resolve(__dirname, '..', 'data', 'sandbox-level-6.html');
  const browser = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const page = await browser.newPage();
  await page.goto('file://' + html);
  const idxs = idxArg.length ? idxArg.map(Number) : cases.map((_, k) => k);
  let bad = 0;
  for (const k of idxs) {
    await page.fill('#hex-input', cases[k]);
    await page.dispatchEvent('#hex-input', 'input');
    await page.fill('#hex-solution', progs[k]);
    await page.dispatchEvent('#hex-solution', 'input');
    await page.waitForTimeout(400);
    const txt = await page.innerText('#stats-hex');
    const ok = txt.includes('Valid') && !txt.includes('Invalid');
    if (!ok) bad++;
    console.log(k, ok ? 'OK ' : 'BAD', txt.replace(/\s+/g, ' ').slice(0, 160));
  }
  console.log(bad ? `INVALID ${bad}` : 'ALL VALID');
  await browser.close();
})();
