const fs = require('fs');
const { chromium } = require(process.env.PLAYWRIGHT_CORE || 'playwright-core');

const usage = 'usage: node headless.cjs <url> [console pattern] [timeout ms] [canvas png] [--keys <ms>:<code>[:<hold ms>],...] [--snap <ms>:<png>,...] [--audio] [--autoplay] [--profile <directory>]';

function parseArguments(argv) {
  const options = { positional: [], actions: [], audio: false, autoplay: false, profile: null };
  for (let index = 0; index < argv.length; index += 1) {
    const argument = argv[index];
    if (argument === '--keys') {
      for (const entry of argv[++index].split(',')) {
        const [at, code, hold] = entry.split(':');
        options.actions.push({ at: Number(at), kind: 'down', code });
        options.actions.push({ at: Number(at) + Number(hold || 100), kind: 'up', code });
      }
    } else if (argument === '--snap') {
      for (const entry of argv[++index].split(',')) {
        const separator = entry.indexOf(':');
        options.actions.push({ at: Number(entry.slice(0, separator)), kind: 'snap', file: entry.slice(separator + 1) });
      }
    } else if (argument === '--audio') {
      options.audio = true;
    } else if (argument === '--autoplay') {
      options.autoplay = true;
    } else if (argument === '--profile') {
      options.profile = argv[++index];
    } else {
      options.positional.push(argument);
    }
  }
  options.actions.sort((first, second) => first.at - second.at);
  return options;
}

async function saveCanvas(page, file) {
  const data = await page.evaluate(() => {
    const canvas = document.getElementById('canvas');
    const copy = document.createElement('canvas');
    copy.width = canvas.width;
    copy.height = canvas.height;
    copy.getContext('2d').drawImage(canvas, 0, 0);
    return copy.toDataURL('image/png');
  });
  fs.writeFileSync(file, Buffer.from(data.split(',')[1], 'base64'));
}

const options = parseArguments(process.argv.slice(2));
const [url, pattern, timeout, image] = options.positional;
if (!url) {
  console.error(usage);
  process.exit(2);
}

(async () => {
  const args = ['--enable-unsafe-webgpu', '--enable-features=Vulkan', '--use-angle=vulkan', `--use-vulkan=${process.env.USE_VULKAN || 'swiftshader'}`, '--ignore-gpu-blocklist'];
  if (options.autoplay) args.push('--autoplay-policy=no-user-gesture-required');
  const launch = { executablePath: process.env.CHROMIUM, headless: true, args };
  const browser = options.profile ? null : await chromium.launch(launch);
  const context = browser ? await browser.newContext() : await chromium.launchPersistentContext(options.profile, launch);
  const page = await context.newPage();
  const expression = pattern ? new RegExp(pattern) : null;
  let matched = false;
  const started = Date.now();
  const elapsed = () => ((Date.now() - started) / 1000).toFixed(1);
  page.on('console', (message) => {
    const text = message.text();
    console.log(`[${elapsed()}s] ${text}`);
    if (expression && expression.test(text)) matched = true;
  });
  page.on('pageerror', (error) => console.log('[pageerror] ' + error.message));
  const printAudio = async () => {
    const stats = await page.evaluate(() => globalThis.aps5Platform.audioStats());
    console.log(`[${elapsed()}s] [audio] ${JSON.stringify(stats)}`);
  };
  await page.goto(url);
  const deadline = started + Number(timeout || 60000);
  const pending = [...options.actions];
  while (Date.now() < deadline && ((expression && !matched) || pending.length > 0)) {
    while (pending.length > 0 && Date.now() - started >= pending[0].at) {
      const action = pending.shift();
      if (action.kind === 'down') {
        await page.keyboard.down(action.code);
      } else if (action.kind === 'up') {
        await page.keyboard.up(action.code);
      } else {
        await saveCanvas(page, action.file);
        console.log(`[${elapsed()}s] snap ${action.file}`);
        if (options.audio) await printAudio();
      }
    }
    await page.waitForTimeout(50);
  }
  if (image) {
    await page.waitForTimeout(1000);
    await saveCanvas(page, image);
  }
  if (options.audio) await printAudio();
  await context.close();
  if (browser) await browser.close();
  process.exit((!expression || matched) && pending.length === 0 ? 0 : 1);
})().catch((error) => {
  console.error(error);
  process.exit(1);
});
