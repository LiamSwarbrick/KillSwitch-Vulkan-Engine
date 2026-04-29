#ifndef PHYSICS_COLLISION_FILTERS_BODY_LAYER_FILTER_H
#define PHYSICS_COLLISION_FILTERS_BODY_LAYER_FILTER_H

#include <cstdint>

// Could do this templated, to have a more optimized matrix of layers, but i'll stick to uint8_t, 
// don't think we will need more than 255 layers
class BodyLayerFilter
{
public:
	explicit BodyLayerFilter(uint8_t numLayers = (uint8_t) 16U);
	~BodyLayerFilter();

	bool shouldCollide(uint8_t a, uint8_t b) const;

	void setNumLayers(uint8_t numLayers);

	void setLayerPair(uint8_t a, uint8_t b, bool shouldCollide);
	void enableLayerPair(uint8_t a, uint8_t b);
	void disableLayerPair(uint8_t a, uint8_t b);

private:
	uint8_t m_numLayers = 0;
	bool* m_collisionMatrix = nullptr;
};

#endif // !PHYSICS_COLLISION_FILTERS_BODY_LAYER_FILTER_H
