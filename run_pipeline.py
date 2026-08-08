from google.cloud import aiplatform

aiplatform.init(
    project='ai4i-dataset',
    location='us-west2',
    staging_bucket='gs://normal-op-motor-data'
)

job = aiplatform.CustomTrainingJob(
    display_name='motor-vibration-isoforest',
    script_path='trainer/task.py',
    container_uri='us-docker.pkg.dev/vertex-ai/training/sklearn-cpu.1-0:latest',
    requirements=['google-cloud-storage', 'gcsfs', 'joblib==1.1.1'],
)

job.run(
    args=['--data-path', 'gs://normal-op-motor-data/vibration_log.csv',
          '--model-dir', 'gs://normal-op-motor-data/models/motor-vibration/v1',
          '--contamination', '0.02'],
    replica_count=1,
    machine_type='n1-standard-4',
)

model = aiplatform.Model.upload(
    display_name='motor-vibration-isoforest',
    artifact_uri='gs://normal-op-motor-data/models/motor-vibration/v1',
    serving_container_image_uri='us-docker.pkg.dev/vertex-ai/prediction/sklearn-cpu.1-0:latest',
)

endpoint = aiplatform.Endpoint.create(display_name='motor-vibration-endpoint')
endpoint.deploy(
    model=model, 
    deployed_model_display_name='motor-vibration-v1',
    machine_type='n1-standard-2', 
    min_replica_count=1, 
    max_replica_count=3
)