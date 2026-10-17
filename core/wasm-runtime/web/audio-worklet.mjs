import { AudioMixer } from './audio-mix.mjs';

class Aps5AudioProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.mixer = null;
    this.port.onmessage = (event) => {
      this.mixer = new AudioMixer(event.data.buffer, event.data.address);
      this.port.postMessage('attached');
    };
  }

  process(inputs, outputs) {
    const [left, right] = outputs[0];
    if (this.mixer === null) {
      left.fill(0);
      right.fill(0);
    } else {
      this.mixer.mix(left, right);
    }
    return true;
  }
}

registerProcessor('aps5-audio', Aps5AudioProcessor);
