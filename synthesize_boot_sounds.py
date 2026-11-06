import math
import struct
import wave
import os

SAMPLE_RATE = 16000

def save_wav(filename, samples, sample_rate=SAMPLE_RATE):
    """Save 16-bit mono WAV for high-fidelity PC listening."""
    with wave.open(filename, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        raw_bytes = bytearray()
        for s in samples:
            # s is in range [-1.0, 1.0]
            val = int(max(-1.0, min(1.0, s)) * 32767)
            raw_bytes.extend(struct.pack('<h', val))
        wf.writeframes(raw_bytes)

def generate_zen_bowl(duration=1.65, f0=432.0):
    """
    Sound 1: Pure Tibetan Zen Singing Bowl (432 Hz warm fundamental)
    - Absolutely NO high frequencies above 864 Hz -> Cannot buzz or rattle!
    - Soft raised-cosine mallet attack (30ms) -> No 'snap' or 'tằng'
    - Exponential acoustic resonance decay with 2.5 Hz natural rotational ring
    """
    total_samples = int(duration * SAMPLE_RATE)
    samples = []
    
    # Fundamental 432 Hz, warm sub-octave 216 Hz, octave 864 Hz
    w0 = 2 * math.pi * f0
    w_sub = 2 * math.pi * (f0 / 2)
    w_oct = 2 * math.pi * (f0 * 2)
    w_beat = 2 * math.pi * 2.2 # 2.2 Hz subtle rotational acoustic bowl breathing
    
    attack_samples = int(0.035 * SAMPLE_RATE) # 35ms ultra-soft mallet strike
    
    for i in range(total_samples):
        t = i / SAMPLE_RATE
        
        # Envelope
        if i < attack_samples:
            env = 0.5 * (1.0 - math.cos(math.pi * i / attack_samples))
        else:
            env = math.exp(-2.2 * (t - 0.035))
            
        # Waveform: Pure warm fundamental + gentle sub-harmonic
        s = (0.75 * math.sin(w0 * t) + 
             0.20 * math.sin(w_sub * t) + 
             0.05 * math.sin(w_oct * t))
             
        beating = 1.0 + 0.07 * math.sin(w_beat * t)
        
        sample = s * env * beating
        samples.append(sample)
        
    # Smooth tail landing to exactly 0
    fade_len = int(0.05 * SAMPLE_RATE)
    for k in range(fade_len):
        idx = total_samples - fade_len + k
        factor = 0.5 * (1.0 + math.cos(math.pi * k / fade_len))
        samples[idx] *= factor
        
    # Normalize peak to 0.70 (-3 dB headroom)
    max_val = max(abs(s) for s in samples)
    return [s / max_val for s in samples]

def generate_warm_acoustic(duration=1.75):
    """
    Sound 2: Warm Acoustic Rhodes / Marimba Cascade
    - Three soft notes in F Major 9th chord: F4 (349 Hz) -> A4 (440 Hz) -> C5 (523 Hz)
    - Woody, warm, zero doorbell vibe, luxury boutique feel.
    """
    total_samples = int(duration * SAMPLE_RATE)
    samples = [0.0] * total_samples
    
    notes = [
        # (freq, start_time, duration, amp)
        (349.23, 0.00, 1.40, 0.85), # F4
        (440.00, 0.12, 1.30, 0.92), # A4
        (523.25, 0.25, 1.45, 1.00), # C5
    ]
    
    for freq, start_t, dur, amp in notes:
        start_idx = int(start_t * SAMPLE_RATE)
        dur_samples = int(dur * SAMPLE_RATE)
        end_idx = min(total_samples, start_idx + dur_samples)
        
        w = 2 * math.pi * freq
        w_body = 2 * math.pi * (freq * 2) # soft 2nd harmonic
        
        attack_len = int(0.015 * SAMPLE_RATE) # 15ms soft hammer
        
        for idx in range(start_idx, end_idx):
            n_idx = idx - start_idx
            t_note = n_idx / SAMPLE_RATE
            
            if n_idx < attack_len:
                env = 0.5 * (1.0 - math.cos(math.pi * n_idx / attack_len))
            else:
                env = math.exp(-3.2 * t_note)
                
            sig = math.sin(w * t_note) + 0.15 * math.sin(w_body * t_note) * math.exp(-6.0 * t_note)
            samples[idx] += sig * env * amp
            
    # Smooth tail landing
    fade_len = int(0.05 * SAMPLE_RATE)
    for k in range(fade_len):
        idx = total_samples - fade_len + k
        factor = 0.5 * (1.0 + math.cos(math.pi * k / fade_len))
        samples[idx] *= factor
        
    max_val = max(abs(s) for s in samples)
    return [s / max_val for s in samples]

def generate_minimal_tech(duration=1.5):
    """
    Sound 3: Minimalist Modern Smart Device Jingle (like high-end Scandinavian audio)
    - 2 warm notes: E4 (329.6 Hz) -> B4 (493.9 Hz)
    - Perfectly rounded pure sine with soft analog warmth.
    """
    total_samples = int(duration * SAMPLE_RATE)
    samples = [0.0] * total_samples
    
    notes = [
        (329.63, 0.00, 1.1, 0.70), # E4
        (493.88, 0.16, 1.25, 0.90), # B4
    ]
    
    for freq, start_t, dur, amp in notes:
        start_idx = int(start_t * SAMPLE_RATE)
        dur_samples = int(dur * SAMPLE_RATE)
        end_idx = min(total_samples, start_idx + dur_samples)
        
        w = 2 * math.pi * freq
        attack_len = int(0.025 * SAMPLE_RATE)
        
        for idx in range(start_idx, end_idx):
            n_idx = idx - start_idx
            t_note = n_idx / SAMPLE_RATE
            
            if n_idx < attack_len:
                env = 0.5 * (1.0 - math.cos(math.pi * n_idx / attack_len))
            else:
                env = math.exp(-2.8 * t_note)
                
            sig = math.sin(w * t_note)
            samples[idx] += sig * env * amp
            
    fade_len = int(0.05 * SAMPLE_RATE)
    for k in range(fade_len):
        idx = total_samples - fade_len + k
        factor = 0.5 * (1.0 + math.cos(math.pi * k / fade_len))
        samples[idx] *= factor
        
    max_val = max(abs(s) for s in samples)
    return [s / max_val for s in samples]

def samples_to_c_header(samples, header_path, name="Warm Acoustic Marimba Cascade (Option 2 - Custom Synthesized)"):
    # Convert normalized float [-1.0, 1.0] to unsigned 8-bit [0, 255] centered at 128
    # Peak deviation ±110 steps from 128 -> Range [18 .. 238] (high 8-bit resolution, zero clipping)
    c_bytes = [max(8, min(248, int(round(128 + s * 110)))) for s in samples]
    
    # Enforce exact midpoint 128 at the end
    c_bytes[-1] = 128
    c_bytes[-2] = 128
    
    lines = [
        f"// {name} - 16kHz mono unsigned 8-bit (Pure Sine Synthesis, Zero IMD, Zero Rattle)",
        "#pragma once",
        "#include <Arduino.h>",
        "#define BOOT_SOUND_RATE 16000",
        f"const uint32_t BOOT_SOUND_LEN={len(c_bytes)};",
        "const uint8_t BOOT_SOUND[] PROGMEM={"
    ]
    
    # 24 samples per line
    chunk_size = 24
    for i in range(0, len(c_bytes), chunk_size):
        chunk = c_bytes[i:i+chunk_size]
        line = ",".join(str(b) for b in chunk)
        if i + chunk_size < len(c_bytes):
            line += ","
        lines.append(line)
        
    lines.append("};")
    lines.append("")
    
    with open(header_path, 'w', encoding='utf-8') as f:
        f.write("\n".join(lines))
    print(f"Written C header to {header_path} ({len(c_bytes)} samples)")

if __name__ == '__main__':
    print("Synthesizing Warm Acoustic Marimba Cascade into include/boot_sound.h...")
    s2 = generate_warm_acoustic()
    samples_to_c_header(s2, "include/boot_sound.h", "Warm Acoustic Marimba Cascade (Option 2 - Custom Synthesized)")
    print("Done!")
