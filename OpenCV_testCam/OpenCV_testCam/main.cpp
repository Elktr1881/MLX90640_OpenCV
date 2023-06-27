#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <atomic>
#include <cmath>

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace cv;
using namespace std;

atomic<bool> stopReading(false);
Point mousePosition;

namespace varGlobal {string data;}

void SerialReader(ifstream& serial)
{
    string line;
    while (!stopReading) { if (getline(serial, line)) { varGlobal::data = line; } }
}


int minAray = 0;
int maxAray = 0;

int posX = 0;
int posY = 0;

const int Scal = 20;
unsigned int cntArr = 0;

Mat Temp(32, 24, CV_8UC3, Scalar(0, 0, 0));
Mat image(32, 24, CV_8UC3, Scalar(0, 0, 0));
Mat imageLine(32 * Scal, 24 * Scal, CV_8UC3, Scalar(0, 0, 0));

void MainProcessing()
{
    namedWindow("Layar");

    // Skala warna dari biru hingga merah
    Scalar blue(255, 0, 0);
    Scalar cyan(255, 255, 0);
    Scalar green(0, 255, 0);
    Scalar yelow(0, 255, 255);
    Scalar red(0, 0, 255);
    Scalar black(0, 0, 0);
    Scalar white(255, 255, 255);

    while (!stopReading)
    {   
        istringstream parseData(varGlobal::data);
        String token;
        vector<int> dataArray;
        double valueMin = 0;
        double valueMax = 0;
        double valueMid = 0;

        while (getline(parseData, token, ','))
        {
            int value = stoi(token);
            dataArray.push_back(value);
            //dataArray[cntArr] = value;
            //cntArr++;
        }
        //cntArr = 0;
        
        for (int i = 0; i < dataArray.size(); ++i)
        {    
            //cout <<"[" << i%32 <<","<< i/32 << "]" << dataArray[i] << ", " <<endl;
            if (dataArray[minAray] > dataArray[i]) { minAray = i;}
            if (dataArray[maxAray] < dataArray[i]) { maxAray = i;}
            valueMin = static_cast<double>(dataArray[minAray]);
            valueMax = static_cast<double>(dataArray[maxAray]);
            valueMid = (valueMax - valueMin) / 5.0;    
            
            //get color from temperature
            Scalar color;
            if (dataArray[i] < valueMin + valueMid * 0.4) { color = blue; }
            else if (dataArray[i] < valueMin + valueMid * 3) { color = cyan; }
            else if (dataArray[i] < valueMin + valueMid * 3.5) { color = green; }
            else if (dataArray[i] < valueMin + valueMid * 4.8) { color = yelow; }
            else if (dataArray[i] > valueMin + valueMid * 4.8) { color = red; }
            Vec3b bgrColor(color[0], color[1], color[2]);
            image.at<Vec3b>(i % 32, i / 32) = bgrColor;

            //get range temperature
            color = black;
            if (dataArray[i] < 10) { color = white; }
            Vec3b tempColor(color[0], color[1], color[2]);
            Temp.at<Vec3b>(i%32, i/32) = tempColor;
        } 

        //cout << valueMin << " " << valueMid << " " << valueMax << endl;
        
        Mat resizedImage, resizedTemp, blurredImage;

        resize(image, resizedImage, Size(32* Scal, 24* Scal));
        GaussianBlur(resizedImage, blurredImage, Size(15, 15), 40);

        resize(Temp, resizedTemp, Size(32 * Scal, 24 * Scal));
        
        Mat imgThresholded;
        inRange(resizedTemp, white, white, imgThresholded); //Threshold the image

        /*
        //morphological opening (removes small objects from the foreground)
        erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(15, 15)));
        dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(15, 15)));

        //morphological closing (removes small holes from the foreground)
        dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(25, 25)));
        erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(25, 25)));
        bitwise_not(imgThresholded, imgThresholded);
        */

        Moments oMoments = moments(imgThresholded);

        double dM01 = oMoments.m01;
        double dM10 = oMoments.m10;
        double dArea = oMoments.m00;

        //calculate the position of the object
        posX = dM10 / dArea;
        posY = dM01 / dArea;

        //cout << dM01 << " " << dM10 << " " << dArea << " " << posX << " " << posY << endl;

        if (dArea < 60000000)
        {
            if (posX > 50 && posY > 50)
            {
                int area = sqrt(dArea/1000);
                Point startPoint(posX - area, posY - area);
                Point endPoint(posX + area, posY + area);
                rectangle(blurredImage, startPoint, endPoint, Scalar(0, 0, 255), 1);
                putText(blurredImage, "Probably Gas Detected", Point(posX-50, posY+area+10), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1);
            }
            
        }
        

        imshow("Tracehold", blurredImage);
        imshow("Layar", imgThresholded);

        if (waitKey(1) == 'q')
            break;
    }
}

int main()
{
    string portName = "COM4";
    ifstream serial(portName);

    if (!serial.is_open())
    {
        cout << "Gagal membuka port serial!" << endl;
        return -1;
    }

    thread readerThread(SerialReader, ref(serial));
    thread mouseThread(MainProcessing);

    cout << "Tekan Enter untuk menghentikan pembacaan..." << endl;
    cin.ignore();

    stopReading = true;
    readerThread.join();
    mouseThread.join();

    serial.close();

    return 0;
}
