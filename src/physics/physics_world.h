// PhysicsWorld class will be the manager / orchestrator of all the physics

// Same as JoltPhysics, we could use a 2 pass of
//      Integration()
//      Collision()
//      Integration()
//      Collision()
// At a fixed step half the expected frame time, say for 60Hz we use a fixed 1/120 step
// Update() :
//      Apply Forces -- (gravity for now) (and forever)
//      -- Input velocities should be calculated before using this (on the input manager or whatever the #%!@) 
//      Constraints (not for now)
//      Broadphase -- coarse collision check update
//      Narrowphase -- checks collisions
//      Generate contacts -- (explore islands in the future)
//      Solve contacts --
//		Integrate positions --
//      