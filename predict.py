from google.cloud import aiplatform

aiplatform.init(project='ai4i-dataset', location='us-west2')
endpoint = aiplatform.Endpoint('projects/ai4i-dataset/locations/us-west2/endpoints/2776550534134366208')

# a real row from your normal training data
# 1797.71, 0.063, 0.82, 0.072, 0.889, 0.983, 7.44
#rpm, fricX(g), fatX(mm/s), fricY(g), fatY(mm/s), fricZ(g), fatZ(mm/s)
result = endpoint.predict(instances=[[1797.71, 10000, 100000, 10000, 100000, 100000, 10000]])
print(result.predictions)  # expect [1]