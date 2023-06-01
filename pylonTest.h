#ifndef PYLONTEST_H
#define PYLONTEST_H

#include <pylondataprocessing/PylonDataProcessingIncludes.h>
#include <pylon/PylonIncludes.h>
#include <QObject>
#include <QImage>
#include <QThread>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QMutexLocker>
#include <list>

using namespace Pylon;
using namespace Pylon::DataProcessing;
class ResultData{
public:
    ResultData() : hasError(false){}
    CPylonImage image;
    StringList_t decodedBarcodes;
    String_t errorMessage;
    bool hasError;
};
class MyOutputObserver : public IOutputObserver
{
public:
    MyOutputObserver()
        : m_waitObject(WaitObjectEx::Create())
    {
    }

    // Implements IOutputObserver::OutputDataPush.
    // This method is called when an output of the CRecipe pushes data out.
    // The call of the method can be performed by any thread of the thread pool of the recipe.
    void OutputDataPush(
            CRecipe& recipe,
            CVariantContainer valueContainer,
            const CUpdate& update,
            intptr_t userProvidedId) override
    {
        // The following variables are not used here:
        PYLON_UNUSED(recipe);
        PYLON_UNUSED(update);
        PYLON_UNUSED(userProvidedId);

        ResultData currentResultData;

        // Get the result data of the recipe via the output terminal's "Image" pin.
        // The value container is a dictionary/map-like type.
        // Look for the key in the dictionary.
        auto posImage = valueContainer.find("Image");
        // We expect there to be an image
        // because the recipe is set up to behave like this.
        PYLON_ASSERT(posImage != valueContainer.end());
        if (posImage != valueContainer.end())
        {
            // Now we can use the value of the key/value pair.
            const CVariant& value = posImage->second;
            if (!value.HasError())
            {
                currentResultData.image = value.ToImage();
            }
            else
            {
                currentResultData.hasError = true;
                currentResultData.errorMessage = value.GetErrorDescription();
            }
        }

        // Get the data from the Barcodes pin.
        auto posBarcodes = valueContainer.find("Barcodes");
        PYLON_ASSERT(posBarcodes != valueContainer.end());
        if (posBarcodes != valueContainer.end())
        {
            const CVariant& value = posBarcodes->second;
            if (!value.HasError())
            {
                for(size_t i = 0; i < value.GetNumArrayValues(); ++i)
                {
                    const CVariant decodedBarcodeValue = value.GetArrayValue(i);
                    if (!decodedBarcodeValue.HasError())
                    {
                        currentResultData.decodedBarcodes.push_back(decodedBarcodeValue.ToString());
                    }
                    else
                    {
                        currentResultData.hasError = true;
                        currentResultData.errorMessage = value.GetErrorDescription();
                        break;
                    }
                }
            }
            else
            {
                currentResultData.hasError = true;
                currentResultData.errorMessage = value.GetErrorDescription();
            }
        }

        // Add data to the result queue in a thread-safe way.
        {
            AutoLock scopedLock(m_memberLock);
            m_queue.emplace_back(currentResultData);
        }

        // Signal that data is ready.
        m_waitObject.Signal();
    }

    // Get the wait object for waiting for data.
    const WaitObject& GetWaitObject()
    {
        return m_waitObject;
    }

    // Get one result data object from the queue.
    bool GetResultData(ResultData& resultDataOut)
    {
        AutoLock scopedLock(m_memberLock);
        if (m_queue.empty())
        {
            return false;
        }

        resultDataOut = std::move(m_queue.front());
        m_queue.pop_front();
        if (m_queue.empty())
        {
            m_waitObject.Reset();
        }
        return true;
    }

private:
    CLock m_memberLock;        // The member lock is required to ensure
    // thread-safe access to the member variables.
    WaitObjectEx m_waitObject; // Signals that ResultData is available.
    // It is set if m_queue is not empty.
    std::list<ResultData> m_queue;  // The queue of ResultData
};


class pylonTest : public QThread{
    Q_OBJECT
public:
    pylonTest(){
        setenv("PYLON_CAMEMU", "1", true);
        PylonAutoInitTerm init;

    }
    ~pylonTest(){}
    void init(){
        try{
            recipe.DeallocateResources();
            recipe.Unload();

            // Load the recipe file.
            recipe.Load(recipePath.toStdString().c_str());

            // Now we allocate all resources we need. This includes the camera device.
            recipe.PreAllocateResources();

            // Set up correct image path to samples.
            // Note: PYLON_DATAPROCESSING_IMAGES_PATH is a string created by the CMake build files.
            recipe.GetParameters().Get(StringParameterName("MyCamera/@CameraDevice/ImageFilename")).SetValue(imagePath.toStdString().c_str());

            // This is where the output goes.
            recipe.RegisterAllOutputsObserver(&resultCollector, RegistrationMode_Append);

        }catch (const GenericException &e){ qDebug() << e.what();}
    }
    void stop(){
        requestInterruption();
    }

    QMutex* drawLock() const{
        return &imageLock;
    }
    const QImage getImage() const{
        return currentImage;
    }
    const QString getValue() const{
        return currentValue;
    }

protected:
    void run() override{
        try{
            // Start the processing.
            recipe.Start();
            while(!isInterruptionRequested()){
                if(resultCollector.GetWaitObject().Wait(5000)){
                    QMutexLocker lockImage(&imageLock);
                    ResultData result;
                    resultCollector.GetResultData(result);
                    if(!result.hasError){
                        currentImage = QImage(result.image.GetWidth(), result.image.GetHeight(), QImage::Format_RGB32 );
                        void *pBuffer = currentImage.bits();
                        size_t bufferSize = currentImage.bytesPerLine() * result.image.GetWidth();

                        formatConverter.OutputPixelFormat = Pylon::PixelType_BGRA8packed;
                        formatConverter.Convert(pBuffer, bufferSize, result.image);

                        currentValue.clear();
                        for(const auto& barcode : result.decodedBarcodes){
                            currentValue.push_back(QString("\n")+barcode.c_str());
                        }
                        emit grabbed();
                    }
                    lockImage.unlock();
                }
            }
            recipe.Stop();
        }catch(const GenICam_3_1_Basler_pylon::GenericException &e){
            qDebug() << e.what();
        }
    }

signals:
    void grabbed();

private:
    QString imagePath = QDir::currentPath() +"/barcode/";
    QString recipePath = QDir::currentPath() +"/barcode.precipe";
    QImage currentImage;
    QString currentValue;
    mutable QMutex imageLock;
    CImageFormatConverter formatConverter;

    // This object is used for collecting the output data.
    // If placed on the stack, it must be created before the recipe
    // so that it is destroyed after the recipe.
    MyOutputObserver resultCollector;


    // Create a recipe object representing a recipe file created using
    // the pylon Viewer Workbench.
    CRecipe recipe;


};

#endif // PYLONTEST_H
