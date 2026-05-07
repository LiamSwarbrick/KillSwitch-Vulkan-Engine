#include "epa.h"

#include <vector>
#include <set>
#include "SDL3/SDL.h"

struct EPAFace
{
	size_t vIndices[3]; // they don't need to have consistent winding order, just the normal to be pointing outwards
	// we will use indices to do edges for reconstruction
	glm::vec3 normal; // normal facing outwards from the polytope

	float distanceToOrigin;
};

struct EPAEdge
{
	size_t a, b; // polytope's vertex indices

	EPAEdge opposite()
	{
		return { b, a };
	}
};

// For O(log n) access using set instead of O(n)
static bool epa_compareEdges(const EPAEdge& a, const EPAEdge& b)
{
	if (a.a == b.a)
	{
		return a.b < b.b;
	}

	return a.a < b.a;
}

// Adapted from Real-Time Collision Detection p47,48
// Point respect to the triangle is origin, so v2 becomes -a
static void epa_barycentricOrigin(
	const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, // Triangle's vertices
	float& u, float& v, float& w)
{
	glm::vec3 v0 = b - a, v1 = c - a, v2 = -a;

	float d00 = glm::dot(v0, v0);
	float d01 = glm::dot(v0, v1);
	float d11 = glm::dot(v1, v1);
	float d20 = glm::dot(v2, v0);
	float d21 = glm::dot(v2, v1);
	float denom_inv = 1.0f / (d00 * d11 - d01 * d01);

	v = (d11 * d20 - d01 * d21) * denom_inv;
	w = (d00 * d21 - d01 * d20) * denom_inv;
	u = 1.0f - v - w;

	return;
}

static EPAFace epa_makeEPAFace(const std::vector<SimplexPoint>& vertices, const size_t a, const size_t b, const size_t c)
{
	EPAFace res;

	// IF CHOSING CORRECT WINDING ORDER we won't need to check if the distance is negative...
	res.vIndices[0] = a;
	res.vIndices[1] = b;
	res.vIndices[2] = c;

	glm::vec3 ab = vertices[b].point - vertices[a].point,
		ac = vertices[c].point - vertices[a].point;

	// cross(ab, ac).normalize() for a better understanding
	glm::vec3 normal = glm::normalize(glm::cross(ab, ac));
	float distance = glm::dot(normal, vertices[a].point);

	// Just in case we have an incorrect winding order for some reason (and we are in release mode)
	if (distance < 0.0f)
	{
		distance = -distance;
		normal = -normal;
		// Swap to avoid creating more incorrect faces and to calculate correct facing barycentric coordinates
		std::swap(res.vIndices[1], res.vIndices[2]);
	}

	res.distanceToOrigin = distance;
	res.normal = normal;

	return res;
}


static void epa_AddIfEdgesFromFace(std::set<EPAEdge, decltype(&epa_compareEdges)>& edges, const EPAFace& face)
{
	// Check the 3 edges
	// TODO: We could manually unroll the loop to avoid the (i+1)%3
	for (int i = 0; i < 3; i++)
	{
		EPAEdge edgeToAdd;

		edgeToAdd.a = face.vIndices[i];
		edgeToAdd.b = face.vIndices[(i + 1) % 3];

		// Try removing the opposite edge (returns 0 if NOT DELETED)
		if (edges.erase(edgeToAdd.opposite()) == 0)
		{
			// If the opposite is not yet in, add this edge
			edges.insert(edgeToAdd);
		}
	}
}

static void epa_FillEPAResult(const std::vector<SimplexPoint>& vertices, const std::vector<EPAFace>& faces, size_t closestFaceIdx, EPAResult& result)
{
	const SimplexPoint& pa = vertices[faces[closestFaceIdx].vIndices[0]];
	const SimplexPoint& pb = vertices[faces[closestFaceIdx].vIndices[1]];
	const SimplexPoint& pc = vertices[faces[closestFaceIdx].vIndices[2]];

	float u, v, w;
	epa_barycentricOrigin(pa.point, pb.point, pc.point, u, v, w);

	glm::vec3 contactA = pa.supportA * u
						+ pb.supportA * v
						+ pc.supportA * w;

	glm::vec3 contactB = pa.supportB * u
						+ pb.supportB * v
						+ pc.supportB * w;

	result.pointA = contactA;
	result.pointB = contactB;
	result.point = (contactA + contactB) * 0.5f;

	result.normal = -faces[closestFaceIdx].normal; // normal is for some reason facing the other side
	result.depth = faces[closestFaceIdx].distanceToOrigin;
}

const int EPA_MAX_ITERATIONS = 64;
const int EPA_MAX_FACES = 64;
const float EPA_TOLERANCE = 0.001f;

EPAResult epa_runEPA(
	const Shape* shapeA, const glm::vec3& posA, const glm::quat& oriA, 
	const Shape* shapeB, const glm::vec3& posB, const glm::quat& oriB, 
	GJKResult& gjk)
{
	EPAResult result;



	std::vector<SimplexPoint> vertices;
	vertices.reserve(EPA_MAX_FACES); // Instead of EPA_MAX_FACES/2 + 2 (eulers formula)
	// we have max 64 because we are going to be possibly adding vertices without removing those who have no faces remaining
	for (size_t i = 0; i < 4; i++)
	{
		vertices.push_back(gjk.simplex[i]);
	}

	std::vector<EPAFace> faces;
	faces.reserve(EPA_MAX_FACES);

	faces.push_back(epa_makeEPAFace(vertices, 0, 1, 2)); // A,B,C
	faces.push_back(epa_makeEPAFace(vertices, 0, 2, 3)); // A,C,D
	faces.push_back(epa_makeEPAFace(vertices, 0, 3, 1)); // A,D,B
	faces.push_back(epa_makeEPAFace(vertices, 1, 3, 2)); // B,D,C

	// Edges that will be added during face deletion
	std::set<EPAEdge, decltype(&epa_compareEdges)> danglingEdges(epa_compareEdges);
	

	// default to closest being face 0
	size_t closestFaceIdx = 0;
	float closestDistance = faces[0].distanceToOrigin;
	// the search direction
	glm::vec3 direction;

	for (int iteration = 0; iteration < EPA_MAX_ITERATIONS; iteration++)
	{ // Epa iterations

		// Check which face is closest
		closestFaceIdx = 0;
		closestDistance = faces[0].distanceToOrigin;
		for (size_t i = 0; i < faces.size(); i++)
		{
			if (faces[i].distanceToOrigin < closestDistance)
			{
				closestDistance = faces[i].distanceToOrigin;
				closestFaceIdx = i;
			}
		}


		// Get the support point in the direction of the face's normal (bc it's from the origin)
		direction = faces[closestFaceIdx].normal;
		SimplexPoint p = gjk_worldSupport(shapeA, posA, oriA, shapeB, posB, oriB, direction);

		// Check if the new support point is NOT further away
		if ((glm::dot(p.point, direction) - closestDistance) < EPA_TOLERANCE)
		{
			//	IF it is NOT further away: we've found the face
			// Calculate the local point of A and B
			// Fill the EPAResult
			epa_FillEPAResult(vertices, faces, closestFaceIdx, result);

			result.converged = true;

			return result;
		}
		
		//	IF it IS further away: 

		for (size_t i = 0; i < faces.size(); i++)
		{ // Face deletion & edge addition
			// Remove faces that are visible from the point. dot(faces[closest].normal, support-vertices[faces[closest].vIndices[0]]
			// So if the normal points in the same direction as AP(:from vertex A of face to Support Point)
			glm::vec3 vA = vertices[faces[i].vIndices[0]].point;

			glm::vec3 aux = p.point - vA;
			float isVisible = glm::dot(faces[i].normal, aux);
			if (isVisible > 0.0f)
			{
				// Add edges (if their opposite is not found)
				epa_AddIfEdgesFromFace(danglingEdges, faces[i]);

				// Remove the face (back-to-curr pop)
				faces[i] = faces.back();
				faces.pop_back();
				i--; // we have to check the same index because we're deleting the current element
			}
		} // Face deletion & edge addition

		// Add new faces looking at the support point
		// NOTE: no need to care about dangling vertices (they will just become unused)
		// BUT WINDING ORDER HAS TO BE EDGE.A, EDGE.B, SUPPORT POINT (ideally to keep winding order)
		//
		// Add the support point to the vertices before next operation
		vertices.push_back(p);

		for (const EPAEdge& edge : danglingEdges)
		{
			// Last index is always vertices.size()-1 because we already added the 
			faces.push_back(epa_makeEPAFace(vertices, edge.a, edge.b, vertices.size()-1));
		}
		danglingEdges.clear();

	} // Epa iterations

	// Use the closest face found so far out of the iterations

	// Check which face is closest
	for (size_t i = 0; i < faces.size(); i++)
	{
		if (faces[i].distanceToOrigin < closestDistance)
		{
			closestDistance = faces[i].distanceToOrigin;
			closestFaceIdx = i;
		}
	}

	// Get all the points involving the contact
	epa_FillEPAResult(vertices, faces, closestFaceIdx, result);
	
	result.converged = false;

	return result;
}
