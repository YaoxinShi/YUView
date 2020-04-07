﻿/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "PlotViewWidget.h"

#include <QPainter>
#include <cmath>

#define PLOTVIEW_WIDGET_DEBUG_OUTPUT 0
#if PLOTVIEW_WIDGET_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_PLOT(fmt) qDebug() << fmt
#else
#define DEBUG_PLOT(fmt,...) ((void)0)
#endif

const QPoint marginTopLeft(30, 5);
const QPoint marginBottomRight(5, 30);

const int axisMaxValueMargin = 10;
const int tickLength = 5;
const int fadeBoxThickness = 10;

const QColor gridLineMajor(180, 180, 180);
const QColor gridLineMinor(230, 230, 230);

PlotViewWidget::PlotViewWidget(QWidget *parent)
  : MoveAndZoomableView(parent)
{
  this->setModel(&this->dummyModel);

  this->propertiesAxis[0].axis = Axis::X;
  this->propertiesAxis[1].axis = Axis::Y;
}

void PlotViewWidget::setModel(PlotModel *model)
{
  this->model = model;
  if (this->model)
    this->update();
}

void drawTextInCenterOfArea(QPainter &painter, QRect area, QString text)
{
  // Set the QRect where to show the text
  QFont displayFont = painter.font();
  QFontMetrics metrics(displayFont);
  QSize textSize = metrics.size(0, text);

  QRect textRect;
  textRect.setSize(textSize);
  textRect.moveCenter(area.center());

  // Draw a rectangle around the text in white with a black border
  QRect boxRect = textRect + QMargins(5, 5, 5, 5);
  painter.setPen(QPen(Qt::black, 1));
  painter.fillRect(boxRect,Qt::white);
  painter.drawRect(boxRect);

  // Draw the text
  painter.drawText(textRect, Qt::AlignCenter, text);
}

void PlotViewWidget::paintEvent(QPaintEvent *paint_event)
{
  Q_UNUSED(paint_event);

  QPainter painter(this);

  const auto widgetRect = QRectF(this->rect());
  const auto plotRect = QRectF(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);

  DEBUG_PLOT("PlotViewWidget::paintEvent widget " << widgetRect << " plot rect " << plotRect);

  this->updateAxis(plotRect);

  auto valuesX = this->getAxisValuesToShow(Axis::X);
  auto valuesY = this->getAxisValuesToShow(Axis::Y);
  drawGridLines(painter, this->propertiesAxis[0], plotRect, valuesX);
  drawGridLines(painter, this->propertiesAxis[1], plotRect, valuesY);

  this->drawPlot(painter, plotRect);

  drawWhiteBoarders(painter, plotRect, widgetRect);
  drawAxis(painter, plotRect);

  drawAxisTicksAndValues(painter, this->propertiesAxis[0], valuesX);
  drawAxisTicksAndValues(painter, this->propertiesAxis[1], valuesY);

  drawFadeBoxes(painter, plotRect, widgetRect);

  auto drawPoint = plotRect.center() + moveOffset;
  QRectF debugRect;
  debugRect.setSize(QSizeF(20, 20));
  debugRect.moveCenter(drawPoint);
  painter.setBrush(Qt::white);
  painter.setPen(Qt::black);
  painter.drawRect(debugRect);

  // if (!this->model)
  // {
  //   drawTextInCenterOfArea(painter, this->rect(), "Please select an item");
  //   return;
  // }

  // drawTextInCenterOfArea(painter, this->rect(), "Drawing drawing :)");
}

void PlotViewWidget::resizeEvent(QResizeEvent *event)
{
  const auto widgetRect = QRectF(this->rect());
  const auto plotRect = QRectF(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);
  this->updateAxis(plotRect);
  MoveAndZoomableView::resizeEvent(event);
}

void PlotViewWidget::drawWhiteBoarders(QPainter &painter, const QRectF &plotRect, const QRectF &widgetRect)
{
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(plotRect.left(), widgetRect.bottom())));
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(widgetRect.right(), plotRect.top())));
  painter.drawRect(QRectF(QPointF(plotRect.right(), 0), widgetRect.bottomRight()));
  painter.drawRect(QRectF(QPointF(0, plotRect.bottom()), widgetRect.bottomRight()));
}

QList<PlotViewWidget::TickValue> PlotViewWidget::getAxisValuesToShow(const PlotViewWidget::Axis axis) const
{
  const auto &properties = this->propertiesAxis[(axis == Axis::X) ? 0 : 1];
  const auto axisVector = (axis == Axis::X) ? QPointF(1, 0) : QPointF(0, -1);
  const auto axisLengthInPixels = QPointF::dotProduct(properties.line.p2() - properties.line.p1(), axisVector);

  double rangeMin, rangeMax;
  {
    const auto lineStartInPlot = this->convertPixelPosToPlotPos(properties.line.p1());
    rangeMin = (axis == Axis::X) ? lineStartInPlot.x() : lineStartInPlot.y();
    auto lineEndInPlot = this->convertPixelPosToPlotPos(properties.line.p2());
    rangeMax = (axis == Axis::X) ? lineEndInPlot.x() : lineEndInPlot.y();
  }
  const auto valueRange = rangeMax - rangeMin;

  const int minPixelDistanceBetweenValues = 50;
  const auto nrValuesToShowMax = double(axisLengthInPixels) / minPixelDistanceBetweenValues;

  auto nrWholeValuesInRange = int(std::floor(valueRange));

  auto offsetLeft = std::ceil(rangeMin) - rangeMin;
  auto offsetRight = rangeMax - std::floor(rangeMax);
  auto rangeRemainder = valueRange - int(std::floor(valueRange));
  // if (offsetLeft < rangeRemainder && offsetRight < rangeRemainder)
  //   nrWholeValuesInRange++;

  double factorMajor = 1.0;
  while (factorMajor * 10 * nrWholeValuesInRange < nrValuesToShowMax)
    factorMajor *= 10;
  while (factorMajor * nrWholeValuesInRange > nrValuesToShowMax)
    factorMajor /= 10;

  double factorMinor = factorMajor;
  while (factorMinor * nrWholeValuesInRange * 2 < nrValuesToShowMax)
    factorMinor *= 2;

  DEBUG_PLOT("PlotViewWidget::getAxisValuesToShow nrWholeValuesInRange " << nrWholeValuesInRange << " nrValuesToShowMax " << nrValuesToShowMax << " factorMajor " << factorMajor << " factorMinor " << factorMinor);

  auto getValuesForFactor = [rangeMin, rangeMax](double factor)
  {
    QList<double> values;
    int min = std::ceil(rangeMin * factor);
    int max = std::floor(rangeMax * factor);
    for (int i = min; i <= max; i++)
      values.append(double(i) / factor);
    return values;
  };

  auto valuesMajor = getValuesForFactor(factorMajor);
  auto valuesMinor = getValuesForFactor(factorMinor);
  
  QList<TickValue> values;
  for (auto v : valuesMinor)
  {
    const bool isMinor = !valuesMajor.contains(v);
    auto graphPos = (axis == Axis::X) ? QPointF(v, 0) : QPointF(0, v);
    auto pixelPosInWidget = this->convertPlotPosToPixelPos(graphPos);
    auto axisValue = (axis == Axis::X) ? pixelPosInWidget.x() : pixelPosInWidget.y();
    values.append({v, axisValue, isMinor});
  }
  return values;
}

void PlotViewWidget::drawAxis(QPainter &painter, const QRectF &plotRect)
{
  painter.setPen(QPen(Qt::black, 1));
  painter.drawLine(plotRect.bottomLeft(), plotRect.topLeft());
  painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
}

void PlotViewWidget::drawAxisTicksAndValues(QPainter &painter, const PlotViewWidget::AxisProperties &properties, const QList<PlotViewWidget::TickValue> &values)
{
  const auto tickLine = (properties.axis == Axis::X) ? QPointF(0, tickLength) : QPointF(-tickLength, 0);

  QFont displayFont = painter.font();
  QFontMetricsF metrics(displayFont);
  painter.setPen(QPen(Qt::black, 1));
  for (auto v : values)
  {
    //auto pixelPosOnAxis = ((v.value - properties.minValue) / valueRange) * axisLengthInPixels;
    QPointF p = properties.line.p1();
    if (properties.axis == Axis::X)
      p.setX(v.pixelPosInWidget);
    else
      p.setY(v.pixelPosInWidget);
    painter.drawLine(p, p + tickLine);

    auto text = QString("%1").arg(v.value);
    auto textSize = metrics.size(0, text);
    QRectF textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(p);
    if (properties.axis == Axis::X)
      textRect.moveTop(p.y() + tickLength + 2);
    else
      textRect.moveRight(p.x() - tickLength - 2);
    
    painter.drawText(textRect, Qt::AlignCenter, text);
  }
}

void PlotViewWidget::drawGridLines(QPainter &painter, const PlotViewWidget::AxisProperties &properties, const QRectF &plotRect, const QList<TickValue> &values)
{
  auto drawStart = (properties.axis == Axis::X) ? plotRect.topLeft() : plotRect.bottomLeft();
  auto drawEnd = (properties.axis == Axis::X) ? plotRect.bottomLeft() : plotRect.bottomRight();

  for (auto v : values)
  {
    if (properties.axis == Axis::X)
    {
      drawStart.setX(v.pixelPosInWidget);
      drawEnd.setX(v.pixelPosInWidget);
    }
    else
    {
      drawStart.setY(v.pixelPosInWidget);
      drawEnd.setY(v.pixelPosInWidget);
    }

    painter.setPen(v.minorTick ? gridLineMinor : gridLineMajor);
    painter.drawLine(drawStart, drawEnd);
  }
}

void PlotViewWidget::drawFadeBoxes(QPainter &painter, const QRectF plotRect, const QRectF &widgetRect)
{
  QLinearGradient gradient;
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
  gradient.setCoordinateMode(QGradient::ObjectMode);
#else
  gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
#endif
  gradient.setStart(QPointF(0.0, 0.0));

  painter.setPen(Qt::NoPen);

  auto setGradientBrush = [&gradient, &painter](bool inverse) {
    gradient.setColorAt(inverse ? 1 : 0, Qt::white);
    gradient.setColorAt(inverse ? 0 : 1, Qt::transparent);
    painter.setBrush(gradient);
  };

  // Vertival
  gradient.setFinalStop(QPointF(1.0, 0));
  setGradientBrush(false);
  painter.drawRect(QRectF(plotRect.left(), 0, fadeBoxThickness, widgetRect.height()));
  setGradientBrush(true);
  painter.drawRect(QRectF(plotRect.right(), 0, -fadeBoxThickness, widgetRect.height()));

  // Horizontal
  gradient.setFinalStop(QPointF(0.0, 1.0));
  setGradientBrush(true);
  painter.drawRect(QRectF(0, plotRect.bottom(), widgetRect.width(), -fadeBoxThickness));
  setGradientBrush(false);
  painter.drawRect(QRectF(0, plotRect.top(), widgetRect.width(), fadeBoxThickness));
}

void PlotViewWidget::drawPlot(QPainter &painter, const QRectF &plotRect) const
{
  const unsigned int plotIndex = 0;

  if (!model)
    return;

  if (plotIndex >= model->getNrPlots())
    return;

  painter.setBrush(QColor(0, 0, 200, 100));
  painter.setPen(QColor(0, 0, 200));

  const auto zeroPointX = plotRect.bottomLeft().x() + fadeBoxThickness;
  const auto zeroPointY = plotRect.bottomLeft().y() - fadeBoxThickness;

  auto param = model->getPlotParameter(plotIndex);
  if (param.type == PlotModel::PlotType::Bar)
  {
    const auto nrBars = param.xRange.max - param.xRange.min;
    for (size_t i = 0; i < nrBars; i++)
    {
      const auto value = model->getPlotPoint(plotIndex, i);

      auto barTopLeft = this->convertPlotPosToPixelPos(QPointF(value.x - 0.5, value.y));
      auto barBottomRight = this->convertPlotPosToPixelPos(QPointF(value.x + 0.5, 0));

      painter.drawRect(QRectF(barTopLeft, barBottomRight));
    }
  }
}

void PlotViewWidget::updateAxis(const QRectF &plotRect)
{ 
  const QPointF thicknessDirectionX(fadeBoxThickness, 0);
  this->propertiesAxis[0].line = {plotRect.bottomLeft() + thicknessDirectionX, plotRect.bottomRight() - thicknessDirectionX};
  
  const QPointF thicknessDirectionY(0, fadeBoxThickness);
  this->propertiesAxis[1].line = {plotRect.bottomLeft() - thicknessDirectionY, plotRect.topLeft() + thicknessDirectionY};

  this->zoomToPixelsPerValueY = double(zoomToPixelsPerValueX);
  if (this->model)
  {
    const auto &parameters = this->model->getPlotParameter(0);
    const auto rangeY = double(parameters.yRange.max - parameters.yRange.min);
    this->zoomToPixelsPerValueY = (this->propertiesAxis[1].line.p1().y() - this->propertiesAxis[1].line.p2().y()) / rangeY;
  }
  
  DEBUG_PLOT("PlotViewWidget::updateAxis lineX " << this->propertiesAxis[0].line << " lineY " << this->propertiesAxis[1].line);
}

QPointF PlotViewWidget::convertPlotPosToPixelPos(const QPointF &plotPos) const
{
  const auto pixelPosX = this->propertiesAxis[0].line.p1().x() + (plotPos.x() * this->zoomToPixelsPerValueX) * this->zoomFactor + this->moveOffset.x();
  
  if (this->fixYAxis)
  {
    const auto pixelPosY = this->propertiesAxis[1].line.p1().y() - (plotPos.y() * this->zoomToPixelsPerValueY);
    return {pixelPosX, pixelPosY};
  }
  else
  {
    const auto pixelPosY = this->propertiesAxis[1].line.p1().y() - (plotPos.y() * this->zoomToPixelsPerValueY) * this->zoomFactor + this->moveOffset.y();
    return {pixelPosX, pixelPosY};
  }
}

QPointF PlotViewWidget::convertPixelPosToPlotPos(const QPointF &pixelPos) const
{
  const auto valueX = ((pixelPos.x() - this->propertiesAxis[0].line.p1().x() - this->moveOffset.x()) / this->zoomFactor) / this->zoomToPixelsPerValueX;

  if (this->fixYAxis)
  {
    const auto valueY = (- pixelPos.y() + this->propertiesAxis[1].line.p1().y()) / this->zoomToPixelsPerValueY;
    return {valueX, valueY};
  }
  else
  {
    const auto valueY = ((- pixelPos.y() + this->propertiesAxis[1].line.p1().y() + this->moveOffset.y()) / this->zoomFactor) / this->zoomToPixelsPerValueY;
    return {valueX, valueY};
  }
}
