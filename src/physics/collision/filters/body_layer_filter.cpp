#include "body_layer_filter.h"

BodyLayerFilter::BodyLayerFilter(uint8_t numLayers)
{
	m_numLayers = numLayers;
	m_collisionMatrix = new bool[numLayers * numLayers];
}

BodyLayerFilter::~BodyLayerFilter()
{
	delete m_collisionMatrix;
	m_collisionMatrix = nullptr;
}

bool BodyLayerFilter::shouldCollide(uint8_t a, uint8_t b) const
{
	return m_collisionMatrix[a * m_numLayers + b];
}

// Careful: This will delete the current collision matrix and create an empty one with the new size
void BodyLayerFilter::setNumLayers(uint8_t numLayers)
{
	if (m_collisionMatrix)
		delete m_collisionMatrix; // Should probably get an aux* and copy to the new aux* instead of deleting and creating an empty one

	m_numLayers = numLayers;
	m_collisionMatrix = new bool[numLayers * numLayers];
}

void BodyLayerFilter::setLayerPair(uint8_t a, uint8_t b, bool shouldCollide)
{
	m_collisionMatrix[a * m_numLayers + b] = shouldCollide;
	m_collisionMatrix[b * m_numLayers + a] = shouldCollide;
}

void BodyLayerFilter::enableLayerPair(uint8_t a, uint8_t b)
{
	m_collisionMatrix[a * m_numLayers + b] = true;
	m_collisionMatrix[b * m_numLayers + a] = true;
}

void BodyLayerFilter::disableLayerPair(uint8_t a, uint8_t b)
{
	m_collisionMatrix[a * m_numLayers + b] = false;
	m_collisionMatrix[b * m_numLayers + a] = false;
}
