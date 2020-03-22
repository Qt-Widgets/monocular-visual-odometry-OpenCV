#include "MainWindow.h"

#include "Utilities.h"

#include "Frame.h"
#include "FrameCorrespondence.h"

#include <QDir>

#include <opencv2/videoio.hpp>
#include <iostream>

#include <Qt3DExtras>
#include <Qt3DRender>
#include <Qt3DCore>


using namespace cv;
using namespace std;

MainWindow::MainWindow
(
	QWidget* parent
):
	QMainWindow(parent),
	_currentFrameIndex(0),
	_numberOfFrames(0),
	_trainSpeedsFile(":/data/train.txt"),
    _trainToObservedFile("trainandobservations.csv")
{
	setupUi(this);

    QString videoPath = QCoreApplication::applicationDirPath() + "/train.mp4";
    loadVideo(videoPath);
	build3dScene();

/*	processNextFrame();
	processNextFrame();

    _frameComparisonWidget->setFrames(&_correspondences[0]);
    addTrackedCameraPoint(_translationByFrame[0]);
	*/
    _timer.setInterval(50);
    connect(&_timer, &QTimer::timeout, [this]()
    {
		if (_currentFrameIndex < 6)
		{
			processNextFrame();
			if (1 < _currentFrameIndex && _currentFrameIndex <= _frames.size())
			{
				_frameComparisonWidget->setFrames(&_correspondences[_currentFrameIndex - 2]);
				addTrackedCameraPoint(_translationByFrame[_currentFrameIndex - 1]);
			}
		}
    });
	_timer.start();
}

void MainWindow::build3dScene()
{
	_3dWindow = new Qt3DExtras::Qt3DWindow();
	_3dWindow->setTitle("3d View");
	_3dWindow->defaultFrameGraph()->setClearColor(QColor(0, 0, 0));

	Qt3DRender::QCamera *cameraEntity = _3dWindow->camera();

	cameraEntity->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
	cameraEntity->setPosition(QVector3D(0, 0, 20.0f));
	cameraEntity->setUpVector(QVector3D(0, 1, 0));
	cameraEntity->setViewCenter(QVector3D(0, 0, 0));

	_rootEntity = new Qt3DCore::QEntity(nullptr);

	Qt3DCore::QEntity* lightEntity = new Qt3DCore::QEntity(_rootEntity);
	Qt3DRender::QPointLight* light = new Qt3DRender::QPointLight(lightEntity);
	light->setColor("white");
	light->setIntensity(1);
	lightEntity->addComponent(light);
    Qt3DCore::QTransform* lightTransform = new Qt3DCore::QTransform(lightEntity);
	lightTransform->setTranslation(cameraEntity->position());
	lightEntity->addComponent(lightTransform);

	// For camera controls
	Qt3DExtras::QFirstPersonCameraController* camController = new Qt3DExtras::QFirstPersonCameraController(_rootEntity);
	camController->setCamera(cameraEntity);

	_3dWindow->setRootEntity(_rootEntity);
	_3dWindow->show();
}

void MainWindow::addTrackedCameraPoint
(
	const QVector3D& point
)
{
	Qt3DCore::QEntity* sphereEntity = new Qt3DCore::QEntity(_rootEntity);

	Qt3DExtras::QSphereMesh* sphereMesh = new Qt3DExtras::QSphereMesh(_rootEntity);
	sphereMesh->setRadius(0.10);

	Qt3DCore::QTransform* sphereTransform = new Qt3DCore::QTransform();
	sphereTransform->setTranslation(point);
	sphereTransform->setScale(1.0);
	sphereTransform->setRotationX(0);
	sphereTransform->setRotationY(0);
	sphereTransform->setRotationZ(0);

	Qt3DExtras::QPhongMaterial* sphereMaterial = new Qt3DExtras::QPhongMaterial();
	sphereMaterial->setDiffuse(QColor(255, 255, 255));

	sphereEntity->addComponent(sphereMesh);
	sphereEntity->addComponent(sphereTransform);
	sphereEntity->addComponent(sphereMaterial);
}

void MainWindow::addTrackedWorldPoint
(
	const QVector3D& point,
	QColor pointColor
)
{
    Qt3DCore::QEntity* sphereEntity = new Qt3DCore::QEntity(_rootEntity);

    Qt3DExtras::QSphereMesh* sphereMesh = new Qt3DExtras::QSphereMesh(_rootEntity);
    sphereMesh->setRadius(0.1);

    Qt3DCore::QTransform* sphereTransform = new Qt3DCore::QTransform();
    sphereTransform->setTranslation(point);
    sphereTransform->setScale(1.0);
    sphereTransform->setRotationX(0);
    sphereTransform->setRotationY(0);
    sphereTransform->setRotationZ(0);

    Qt3DExtras::QPhongMaterial* sphereMaterial = new Qt3DExtras::QPhongMaterial();
    sphereMaterial->setDiffuse(pointColor);

    sphereEntity->addComponent(sphereMesh);
    sphereEntity->addComponent(sphereTransform);
    sphereEntity->addComponent(sphereMaterial);
}

void MainWindow::loadVideo
(
	const QString& path
)
{
	_frames.clear();
	_correspondences.clear();
	_translationByFrame.clear();
	_translationByFrame.append(QVector3D(0, 0, 0));

	//_videoCapture = VideoCapture(0);
	_videoCapture = VideoCapture(path.toStdString(), CAP_ANY);
    if (_videoCapture.isOpened())
    {
        _numberOfFrames = static_cast<int>(_videoCapture.get(CAP_PROP_FRAME_COUNT));

		_frames.reserve(_numberOfFrames);
		_correspondences.reserve(_numberOfFrames);

        _trainSpeedsFile.open(QIODevice::ReadOnly);
        if (!_trainToObservedFile.open(QFile::WriteOnly | QFile::Text))
        {
            cout << "Failed to open observations file" << endl;
        }
        else
        {
            cout << "Failed to open video." << endl;
        }
    }
}

void MainWindow::processNextFrame()
{
	Mat cvFrame;
    _videoCapture >> cvFrame;
	if (cvFrame.empty() == false)
	{
        Frame frame;
		frame.extractFeatures(cvFrame);
        frame.setRecordedSpeed(_trainSpeedsFile.readLine().toFloat());
		_frames.append(frame);

		if (_frames.count() > 1)
		{
			FrameCorrespondence correspondence;
			correspondence.findMatches(_frames[_frames.count() - 2], _frames[_frames.count() - 1]);
			correspondence.extrapolateMatrices();

			QVector3D translation = matToVector(correspondence.rotation() * correspondence.translation());
			_translationByFrame.append(_translationByFrame.last() + translation);

			int pointIndex = 0;
            for (auto worldPoint : matToVectorList(correspondence.worldCoords()))
            {
				auto point2d = correspondence.firstFrame()->extractedFeatures()[correspondence.goodMatches()[pointIndex].queryIdx].pt;
                addTrackedWorldPoint(worldPoint + _translationByFrame.last(), colorForPoint(point2d, frame.width(), frame.height()));
				pointIndex++;
            }

            float worldTravelDistance = sqrt((_translationByFrame.end() - 2)->distanceToPoint(*(_translationByFrame.end() - 1)));
            _frames[_frames.count() - 2].setObservedSpeed(worldTravelDistance);
            _correspondences.append(correspondence);
            _trainToObservedFile.write(QString("%1, %2\n").arg(_frames[_frames.count() - 2].recordedSpeed()).arg(_frames[_frames.count() - 2].observedSpeed()).toLatin1());
		}

		_currentFrameIndex++;
		cvFrame.release();
	}
}

void MainWindow::finalize()
{
	_videoCapture.release();
	_trainSpeedsFile.close();
	_trainToObservedFile.close();
}
