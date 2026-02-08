import cv2, struct, numpy as np

WIDTH, HEIGHT = 640, 480
THRESHOLD = 12  # Czułość (wyższa = mniejszy plik)
KEYFRAME_INTERVAL = 100 

def convert(input_path):
    cap = cv2.VideoCapture(input_path)
    frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    last_img = np.zeros((HEIGHT, WIDTH, 3), dtype=np.uint8)
    
    with open("movie_pro.vid", "wb") as f:
        f.write(b"AMSP") # Magic: AMS Pro
        f.write(struct.pack("III", WIDTH, HEIGHT, frames))
        
        for i in range(frames):
            ret, frame = cap.read()
            if not ret: break
            frame = cv2.resize(frame, (WIDTH, HEIGHT))
            
            is_keyframe = (i % KEYFRAME_INTERVAL == 0)
            
            if is_keyframe:
                # [0xFFFFFFFF][Surowe dane BGR]
                payload = struct.pack("I", 0xFFFFFFFF) + frame.tobytes()
            else:
                # Oblicz różnicę
                diff = cv2.absdiff(frame, last_img)
                mask = np.any(diff > THRESHOLD, axis=2)
                y_coords, x_coords = np.where(mask)
                
                # Paczka zmian: [Liczba zmian][ (Index, B, G, R)... ]
                changes = []
                for y, x in zip(y_coords, x_coords):
                    changes.append(struct.pack("IBBB", y * WIDTH + x, *frame[y, x]))
                
                payload = struct.pack("I", len(y_coords)) + b"".join(changes)
            
            # Zapisz rozmiar paczki i dane
            f.write(struct.pack("I", len(payload)))
            f.write(payload)
            
            last_img = frame.copy()
            if i % 100 == 0: print(f"Klatka {i}/{frames}")

if __name__ == "__main__":
    #podane przez użytkownika np python3 conventer.py input_video.mp4
    import sys
    if len(sys.argv) != 2:
        print("Użycie: python3 conventer.py input_video.mp4")
    else:
        convert(sys.argv[1])

        