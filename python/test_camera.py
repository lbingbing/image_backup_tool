import cv2

url = 1
#url = 'http://192.168.31.216:8080/video'
stop = False
while not stop:
    capture = cv2.VideoCapture(url)
    width, height = capture.get(3), capture.get(4)
    #capture.set(cv2.CAP_PROP_FRAME_WIDTH, width * 2)
    #capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height * 2)
    print((width, height))
    while True:
        ret, frame = capture.read()
        cv2.imshow('frame', frame)
        #print(frame.shape)
        key = cv2.waitKey(1)
        if key == ord('q'):
            stop = True
            break
        elif key == ord('s'):
            cv2.imwrite('z.jpg', frame)
