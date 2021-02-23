# Regenie

## regenie_step1,  regenie_step2_v1,  regenie_step2_v2

To run using Docker:

  1. cd regenie
  2. make docker-build
  3. docker images      # Get the repo name and tag number
  4. Use vim to replace "regenie:v2.0.1" in the first line of each file with the repo name and tag number located in step 3.
  5. Use vim to replace "/Users/.../regenie" in the first line of each file with the working directory for where the regenie directory is located locally.
  6. Type in the terminal "./regenie_step1", "./regenie_step2_v1", "./regenie_step2_v2"
