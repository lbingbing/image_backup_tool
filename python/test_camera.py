import cv2 as cv

url = 1
#url = 'http://192.168.31.216:8080/video'
stop = False
while not stop:
    capture = cv.VideoCapture(url)
    width, height = capture.get(3), capture.get(4)
    #capture.set(cv.CAP_PROP_FRAME_WIDTH, width * 2)
    #capture.set(cv.CAP_PROP_FRAME_HEIGHT, height * 2)
    print((width, height))
    while True:
        ret, frame = capture.read()
        cv.imshow('frame', frame)
        #print(frame.shape)
        key = cv.waitKey(1)
        if key == ord('q'):
            stop = True
            break
        elif key == ord('s'):
            cv.imwrite('z.jpg', frame)
