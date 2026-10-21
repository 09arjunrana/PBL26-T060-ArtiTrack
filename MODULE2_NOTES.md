# Module 2 — OOP Hierarchy & Preservation Notes

## Class Hierarchy & Design
- **Base Class**: `Artifact` handles common fields (`id`, `name`, `age`, `condition`, `location`).
- **Inherited Types**:
  - `PotteryArtifact`: Incorporates porosity factor into priority calculations.
  - `CoinArtifact`: Incorporates material/metal composition.
  - `AncientCoin`: Extends `CoinArtifact` with dynasty-specific historical weighting.
  - `Weapon`: Includes rust assessment.
  - `Sculpture`: Tracks structural crack severity.
  - `Manuscript`: Evaluates environmental decay rate.
- **Mixin**: `Restorable` provides virtual interface methods for tailored conservation advice.

## Operators
- Comparison operators (`<`, `>`, `==`) compare artifact preservation priority scores.
- Stream operator (`<<`) formats clean command-line display output.
