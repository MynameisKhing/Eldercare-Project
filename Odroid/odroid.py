import cv2  
import mediapipe as mp  
import threading  
import time  
from PIL import Image  
import pytesseract  
import speech_recognition as sr  
import serial  


from gtts import gTTS  
import os  


import psutil  
import subprocess  


import numpy as np  
import requests, urllib.parse  
import io  


arduino_serial = serial.Serial('/dev/ttyACM0', 9600, timeout = 1)
time.sleep(2)  

nodemcu_serial = serial.Serial('/dev/ttyUSB0', 9600, timeout = 1)
time.sleep(2)  


mp_pose = mp.solutions.pose
pose = mp_pose.Pose()
mp_drawing = mp.solutions.drawing_utils


cap = cv2.VideoCapture(0)  
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1080)  
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720) 


frame_lock = threading.Lock()  
frame = None  
stop_threads = False  
mode = 1  
OCR = False  
text_received = "default text"  
notification = False  


mode_lock = threading.Lock()

def sent_line_notify(massage, image_path):
    token = "qFcOGWcjUWRbxjMnCqTuakORly91d5i2dGU6mAZnt7H"  
    url = 'https://notify-api.line.me/api/notify'  
    HEADERS = {'Authorization': 'Bearer ' + token}  

    msg = massage
    # ภาพ
    img = Image.open(image_path)  
    img.load()  
    myimg = np.array(img)  
    f = io.BytesIO()  
    Image.fromarray(myimg).save(f, 'png')  
    data = f.getvalue()  

    response = requests.post(url, headers=HEADERS, params={"message": msg}, files={"imageFile": data})
    print(response)  

def speak_thai_google(text):
    
    tts = gTTS(text=text, lang='th')
    tts.save("thai_speech_mono.mp3")  

    os.system("ffmpeg -i thai_speech_mono.mp3 -ac 2 thai_speech_stereo.mp3 -y")

    os.system("mpg321 thai_speech_stereo.mp3")

speak_thai_google("สวัสดีค่ะนายท่านยินดีต้อนรับสู่ Elder Care System ตอนนี้กำลังเตรียมความพร้อมระบบ กรุณารอสักครู่")

def capture_frames():
    global frame, stop_threads  
    while not stop_threads:  
        ret, current_frame = cap.read()  
        if ret:  
            with frame_lock:  
                frame = current_frame  
        time.sleep(0.01)  

def process_frames():
    global frame, stop_threads, mode, OCR  
    last_time = 0  

    def speak_thai_google(text):
        tts = gTTS(text=text, lang='th')  
        tts.save("thai_speech_mono.mp3")  
        os.system("ffmpeg -i thai_speech_mono.mp3 -ac 2 thai_speech_stereo.mp3 -y")
        os.system("mpg321 thai_speech_stereo.mp3")

    while not stop_threads:  
        start_time = time.time()  

        if frame is None:  
            continue  

        with frame_lock:  
            frame_copy = frame.copy()  

        if mode == 1:  
           
            image = cv2.cvtColor(frame_copy, cv2.COLOR_BGR2RGB)
            results = pose.process(image)  

            image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)

            if results.pose_landmarks:  
                mp_drawing.draw_landmarks(image, results.pose_landmarks, mp_pose.POSE_CONNECTIONS)  

                landmarks = results.pose_landmarks.landmark  
                landmark_coords = [(int(landmark.x * frame_copy.shape[1]), int(landmark.y * frame_copy.shape[0])) for landmark in landmarks]

                x_values = [coord[0] for coord in landmark_coords]  
                y_values = [coord[1] for coord in landmark_coords]  

                x_min, x_max = min(x_values), max(x_values)  
                y_min, y_max = min(y_values), max(y_values)  

                label = "Person"  
                color = (0, 255, 0)  

                nose_y = landmarks[mp_pose.PoseLandmark.NOSE].y
                left_hip_y = landmarks[mp_pose.PoseLandmark.LEFT_HIP].y
                right_hip_y = landmarks[mp_pose.PoseLandmark.RIGHT_HIP].y
                left_ankle_y = landmarks[mp_pose.PoseLandmark.LEFT_ANKLE].y
                right_ankle_y = landmarks[mp_pose.PoseLandmark.RIGHT_ANKLE].y

                if (left_ankle_y - nose_y) < 0.2 and (right_ankle_y - nose_y) < 0.2:
                    label = "Fall Detected"  
                    color = (0, 0, 255)  
                    if time.time() - last_time > 60:  
                        print("Detected a fall!")  
                        cv2.imwrite('Fall_image.jpg', image)  
                        sent_line_notify("เเจ้งเตือนการหกล้ม", "Fall_image.jpg")  
                        last_time = time.time()  

                cv2.putText(image, label, (x_min, y_min - 10), cv2.FONT_HERSHEY_SIMPLEX, 1, color, 2, cv2.LINE_AA)
                cv2.rectangle(image, (x_min, y_min), (x_max, y_max), color, 2)

            frame_time = time.time() - start_time  
            time_text = f"Frame time: {frame_time:2f} sec"  
            cv2.putText(image, time_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (56, 61, 150), 2, cv2.LINE_AA)  
            cv2.imshow('Pose Detection with Bounding Box', image)

        if cv2.waitKey(10) & 0xFF == ord('q'): 
            stop_threads = True  
            break  

        
        if mode == 2:  
            image = cv2.cvtColor(frame_copy, cv2.COLOR_BGR2RGB)  
            image = cv2.cvtColor(image, cv2.COLOR_RGB2BGR)

            if cv2.waitKey(10) & 0xFF == ord('q'):
                stop_threads = True 
                break
            
            if OCR == True: 
                print("Processing . . .") 
                cv2.imwrite('captured_image.jpg', image)  

                image = cv2.imread('captured_image.jpg')

                gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

                thresh = cv2.threshold(gray, 150, 255, cv2.THRESH_BINARY)[1]

                processed_image = Image.fromarray(thresh)
                                                                                                                                                                                                                        
                text = pytesseract.image_to_string(image, lang='tha+eng')

                print(text)  
                if text.strip():
                    speak_thai_google(text)
                else:
                    print("Error: No text provided for TTS.")

                OCR = False  

            frame_time = time.time() - start_time  
            time_text = f"Frame time: {frame_time:2f} sec"  
            cv2.putText(image, time_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (56, 61, 150), 2, cv2.LINE_AA)  
            cv2.imshow('Pose Detection with Bounding Box', image)

def change_mode_serial():
    global mode, stop_threads, OCR, notification  
    while not stop_threads:  
        if arduino_serial.in_waiting > 0:  
            arduino_data = arduino_serial.readline().decode('utf-8').strip()  
            #print(f"Arduino send: {arduino_data}\n")  
            
            if("NOTIFICATION:" in arduino_data): 
                print("Send NOTIFICATION")
                notification = True 
                nodemcu_serial.write((arduino_data + "\n").encode('utf-8'))  
            if(arduino_data == "STOP_ALARM"):
                notification = False
            if(arduino_data == "MODE:1"):
                mode = 1 
                nodemcu_serial.write((arduino_data + "\n").encode('utf-8'))  
                print("Mode changed Fall detection.")  
            if(arduino_data == "MODE:2"):
                mode = 2 
                nodemcu_serial.write((arduino_data + "\n").encode('utf-8')) 
                print("Mode changed to OCR.")
            if(arduino_data == "Capture"):
                OCR = True  
                print("Entered picture from webcam.")  
            if(arduino_data == "Stop"):  
                stop_threads = True  
                print("Exiting...")  
                break 

        if nodemcu_serial.in_waiting > 0:  
            nodemcu_data = nodemcu_serial.readline().decode('utf-8').strip()  
            #print(f"NodeMCU send: {nodemcu_data}\n")  
            
            if(nodemcu_data == "WIFI"):  
                arduino_serial.write((nodemcu_data + "\n").encode('utf-8'))  
            if(nodemcu_data == "LOST"): 
                arduino_serial.write((nodemcu_data + "\n").encode('utf-8'))  
            if(nodemcu_data == "MODE:1"):  
                mode = 1  
                arduino_serial.write((nodemcu_data + "\n").encode('utf-8'))  
                print("Mode changed Fall detection.")  
            if(nodemcu_data == "MODE:2"):  
                mode = 2  
                arduino_serial.write((nodemcu_data + "\n").encode('utf-8'))  
                print("Mode changed to OCR.")  
            if(nodemcu_data == "Capture"):  
                OCR = True  
                print("Entered picture from webcam.")  
            if(nodemcu_data == "Stop"):  
                stop_threads = True  
                print("Exiting...")  
                break 


def voice_mode():
    global mode, stop_threads, text_received, OCR, notification, frame  
    recognizer = sr.Recognizer()  

    while not stop_threads:  
        try:
            with sr.Microphone() as source:  
                #print("Adjusting for ambient noise...")  
                recognizer.adjust_for_ambient_noise(source)  
                print("Say something (5 seconds limit)...")  
                audio = recognizer.listen(source, phrase_time_limit=5)  

            text_received = recognizer.recognize_google(audio).lower()  
            print("You said: " + text_received)  

            # ตรวจสอบคำสั่งเสียงที่ได้รับและทำการเปลี่ยนโหมด
            if "1" in text_received or "one" in text_received:  
                mode = 1  
                arduino_serial.write(("MODE:1\n").encode('utf-8'))  
                nodemcu_serial.write(("MODE:1\n").encode('utf-8')) 
                print("Mode changed to Fall detection.")  
            if "2" in text_received or "two" in text_received or "to" in text_received:  
                mode = 2  
                arduino_serial.write(("MODE:2\n").encode('utf-8'))  
                nodemcu_serial.write(("MODE:2\n").encode('utf-8'))  
                print("Mode changed to OCR.")  
            if "capture" in text_received:  
                OCR = True  
                print("Entered picture from webcam.")  
            if "help" in text_received:  
                cv2.imwrite('Help_image.jpg', frame)  
                sent_line_notify("ขอความช่วยเหลือ", "Help_image.jpg")  
                print("Call for help.")  
            if "quit" in text_received:  
                stop_threads = True  
                print("Exiting via voice command.")  
                break  

        except sr.UnknownValueError:  
            print("Sorry, I didn't understand that.")  
        except sr.RequestError as e:  
            print(f"Error with Google Web Speech API: {e}")  


def monitor_mode(): 
    global stop_threads  
    def get_cpu_usage():
        return psutil.cpu_percent(interval=1)  

    def get_memory_usage():
        memory = psutil.virtual_memory()  
        return memory.percent  

    def get_temperature():
        try:
            temps = psutil.sensors_temperatures()  
            if not temps:
                return "No temperature sensors found."  

            for name, entries in temps.items():  
                for entry in entries:
                    return entry.current  
        except Exception as e:
            return "Error in temperature monitoring."  

    def sent_system_stats():
        odroid_info = f"CPU:{get_cpu_usage()},RAM:{get_memory_usage()},TEMP:{get_temperature()}\n"  
        nodemcu_serial.write(odroid_info.encode('utf-8'))  
        arduino_serial.write(odroid_info.encode('utf-8'))  

    while not stop_threads:
        sent_system_stats() 

def alarm_clock():
    global stop_threads, notification  
    while not stop_threads:
        if notification == True:  
            os.system("mpg321 screaming-monkeys.mp3")  
        if notification == True:  
            os.system("gorilla-tag-monkeys.mp3")  
        if notification == True:  
            os.system("mpg321 monkeys_HJucFGX.mp3")  

capture_thread = threading.Thread(target=capture_frames)  
process_thread = threading.Thread(target=process_frames)  
voice_thread = threading.Thread(target=voice_mode)  
serial_thread = threading.Thread(target=change_mode_serial)  
monitor_thread = threading.Thread(target=monitor_mode)  
alarm_thread = threading.Thread(target=alarm_clock)  

capture_thread.start()
process_thread.start()
voice_thread.start()
serial_thread.start()
monitor_thread.start()
alarm_thread.start()

capture_thread.join()
process_thread.join()
voice_thread.join()
serial_thread.join()
monitor_thread.join()
alarm_thread.join()

cap.release() 
cv2.destroyAllWindows()  