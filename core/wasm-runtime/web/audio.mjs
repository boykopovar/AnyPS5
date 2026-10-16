import { AudioMixer } from './audio-mix.mjs';

export function startAudio(target) {
  const context = new AudioContext({ sampleRate: 48000, latencyHint: 'interactive' });
  const node = context.audioWorklet.addModule(new URL('./audio-worklet.mjs', import.meta.url)).then(() => {
    const created = new AudioWorkletNode(context, 'aps5-audio', { numberOfInputs: 0, outputChannelCount: [2] });
    created.connect(context.destination);
    return created;
  });
  let mixer = null;
  let attached = false;
  const publish = () => {
    if (mixer !== null) Atomics.store(mixer.state, 0, attached && context.state === 'running' ? 1 : 0);
  };
  context.addEventListener('statechange', publish);
  const resume = () => {
    if (context.state === 'suspended') context.resume().catch((error) => console.log('anyps5 audio: ' + error));
  };
  target.addEventListener('pointerdown', resume);
  target.addEventListener('keydown', resume);
  return {
    async attach(buffer, address) {
      mixer = new AudioMixer(buffer, address);
      const worklet = await node;
      worklet.port.onmessage = (event) => {
        if (event.data !== 'attached') return;
        attached = true;
        publish();
      };
      worklet.port.postMessage({ buffer, address });
    },
    stats() {
      const counters = mixer === null ? { running: 0, active: 0, consumed: 0, underruns: 0 } : mixer.stats();
      return { state: context.state, ...counters };
    },
  };
}
