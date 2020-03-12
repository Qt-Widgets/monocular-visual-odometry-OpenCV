#ifndef VIDEOFRAMEWIDGET_H
#define VIDEOFRAMEWIDGET_H

#include <QWidget>

class Frame;
class FrameCorrespondence;

class FrameComparisonWidget : public QWidget
{
public:
	FrameComparisonWidget(QWidget* parent = nullptr);

	virtual void paintEvent(QPaintEvent* event) override;

	void setFrames(Frame* left, Frame* right, FrameCorrespondence* correspondence);

private:
	Frame* _leftFrame;
	Frame* _rightFrame;
	FrameCorrespondence* _correspondence;

	bool _showEpilines;
	bool _showFeatures;
	bool _showFeatureMatches;
};

#endif // VIDEOFRAMEWIDGET_H