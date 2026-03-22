import os
import sys

# --- FORCED GUI PATH PATCH ---
os.environ['TCL_LIBRARY'] = r'C:\Users\apsar\AppData\Local\Programs\Python\Python310\tcl\tcl8.6'
os.environ['TK_LIBRARY'] = r'C:\Users\apsar\AppData\Local\Programs\Python\Python310\tcl\tk8.6'

import csv
import cv2
import numpy as np
from PIL import Image
import pandas as pd
import time
import serial
import tkinter as tk

print("Starting Next-Gen EVIS script...")

# ---------------- SERIAL CONNECTION ----------------
try:
    esp32 = serial.Serial('COM3', 115200, timeout=1)
    time.sleep(2)
    print("ESP32 Connected Successfully")
except Exception as e:
    esp32 = None
    print(f"ESP32 not connected: {e}")

# ---------------- UPDATED PROJECT PATHS ----------------
BASE_PATH = r"C:\Users\apsar\Desktop\project face"

# Paths updated based on your request
TRAINING_IMAGE_PATH = os.path.join(BASE_PATH, "TrainingImage")
USER_DETAILS_PATH = os.path.join(BASE_PATH, "userDetails")

# Derived paths
TRAINING_LABEL_PATH = os.path.join(BASE_PATH, "TrainingImageLabel")
CSV_PATH = os.path.join(USER_DETAILS_PATH, "UserDetails.csv")
TRAINER_PATH = os.path.join(TRAINING_LABEL_PATH, "Trainer.yml")
HAAR_PATH = os.path.join(BASE_PATH, "data", "haarcascade_frontalface_default.xml")

# Create folders if they don't exist
for path in [TRAINING_IMAGE_PATH, TRAINING_LABEL_PATH, USER_DETAILS_PATH]:
    os.makedirs(path, exist_ok=True)

# ---------------- FUNCTIONS ----------------

def TakeImages():
    Id, name = txt.get(), txt2.get()
    if not Id or not name:
        message.config(text="Status: Enter ID and Name!")
        return
    cam = cv2.VideoCapture(0)
    detector = cv2.CascadeClassifier(HAAR_PATH)
    sampleNum = 0
    while True:
        ret, img = cam.read()
        if not ret: break
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        faces = detector.detectMultiScale(gray, 1.3, 5)
        for (x, y, w, h) in faces:
            sampleNum += 1
            cv2.imwrite(f"{TRAINING_IMAGE_PATH}/{name}.{Id}.{sampleNum}.jpg", gray[y:y+h, x:x+w])
            cv2.rectangle(img, (x, y), (x+w, y+h), (0, 255, 0), 2)
        cv2.imshow("Registration", img)
        if cv2.waitKey(1) == ord('q') or sampleNum >= 60: break
    cam.release()
    cv2.destroyAllWindows()
    
    file_exists = os.path.isfile(CSV_PATH)
    with open(CSV_PATH, 'a+', newline='') as f:
        writer = csv.writer(f)
        if not file_exists or os.stat(CSV_PATH).st_size == 0:
            writer.writerow(["Id", "Name"])
        writer.writerow([Id, name])
    message.config(text=f"Status: Saved {name}")

def TrainImages():
    recognizer = cv2.face.LBPHFaceRecognizer_create()
    imagePaths = [os.path.join(TRAINING_IMAGE_PATH, f) for f in os.listdir(TRAINING_IMAGE_PATH)]
    faces, Ids = [], []
    for path in imagePaths:
        img = Image.open(path).convert('L')
        faces.append(np.array(img, 'uint8'))
        try:
            # Extract ID from the filename (e.g., Name.Id.Sample.jpg)
            Ids.append(int(os.path.split(path)[-1].split(".")[1]))
        except: continue
    if not faces:
        message.config(text="Status: No Images!")
        return
    recognizer.train(faces, np.array(Ids))
    recognizer.save(TRAINER_PATH)
    message.config(text="Status: Model Trained")

def StartRecognition():
    if not os.path.exists(TRAINER_PATH):
        message.config(text="Status: Train First!")
        return
    recognizer = cv2.face.LBPHFaceRecognizer_create()
    recognizer.read(TRAINER_PATH)
    faceCascade = cv2.CascadeClassifier(HAAR_PATH)
    
    # Read CSV and clean column names
    df = pd.read_csv(CSV_PATH)
    df.columns = df.columns.str.strip()
    
    cam = cv2.VideoCapture(0)
    while True:
        ret, img = cam.read()
        if not ret: break
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        faces = faceCascade.detectMultiScale(gray, 1.2, 5)
        for (x, y, w, h) in faces:
            result_id, conf = recognizer.predict(gray[y:y+h, x:x+w])
            
            if conf < 70:
                # Force both to string for a reliable match
                try:
                    name = df.loc[df['Id'].astype(str) == str(result_id)]['Name'].values[0]
                except:
                    name = "Authorized"

                print(f"Authorized: {name}")
                if esp32:
                    esp32.write(b'S')
                    esp32.flush()
                    time.sleep(1.5)
                
                cam.release()
                cv2.destroyAllWindows()
                window.destroy()
                return
            else:
                cv2.putText(img, "Unknown", (x, y-10), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0,0,255), 2)
        
        cv2.imshow("Recognition Running", img)
        if cv2.waitKey(1) == ord('q'): break
    cam.release()
    cv2.destroyAllWindows()

# ---------------- GUI ----------------
window = tk.Tk()
window.title("Next-Gen Intelligent Vehicle System")
window.geometry("1000x600")
window.configure(background="#e8f5e9")

header = tk.Label(window, text="Next-Gen Intelligent Vehicle Safety System", bg="#1b5e20", fg="white", font=('times', 25, 'bold'), width=50, height=2)
header.pack()

tk.Label(window, text="ID:", bg="#e8f5e9", font=15).place(x=200, y=200)
txt = tk.Entry(window, width=20, font=15); txt.place(x=350, y=200)
tk.Label(window, text="Name:", bg="#e8f5e9", font=15).place(x=200, y=250)
txt2 = tk.Entry(window, width=20, font=15); txt2.place(x=350, y=250)
message = tk.Label(window, text="Status: Ready", bg="#e8f5e9", fg="#1b5e20", font=('times', 15, 'bold')); message.place(x=350, y=300)

tk.Button(window, text="Register Face", command=TakeImages, bg="#43a047", fg="white", width=15, height=2).place(x=200, y=400)
tk.Button(window, text="Train Model", command=TrainImages, bg="#43a047", fg="white", width=15, height=2).place(x=350, y=400)
tk.Button(window, text="Unlock Vehicle", command=StartRecognition, bg="#1b5e20", fg="white", width=15, height=2).place(x=500, y=400)

window.mainloop()