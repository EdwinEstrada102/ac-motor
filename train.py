from google.cloud import aiplatform
import argparse
import joblib
import pandas as pd
from sklearn.ensemble import IsolationForest
from google.cloud import storage

def main():
    aiplatform.init(
        project='ai4i-dataset',
        location='us-west2',
        staging_bucket='gs://normal-op-motor-data/vibration_log.csv'
    )

    parser = argparse.ArgumentParser()
    parser.add_argument('--data-path', type=str, required=True)
    parser.add_argument('--model-dir', type=str, required=True)
    parser.add_argument('--contamination', type=float, default=0.02)
    args = parser.parse_args()

    df = pd.read_csv(args.data_path)
    feature_cols = ['rms', 'kurtosis', 'peak_amplitude', 'crest_factor', 'freq_band_1', 'freq_band_2']
    X = df[feature_cols]

    model = IsolationForest(
        n_estimators=200,
        contamination=args.contamination,  # expected % of "noisy" points in normal data
        random_state=42
    )
    model.fit(X)

    # Save locally then upload to GCS
    joblib.dump(model, 'model.joblib')
    bucket_name = args.model_dir.replace('gs://', '').split('/')[0]
    blob_path = '/'.join(args.model_dir.replace('gs://', '').split('/')[1:]) + '/model.joblib'
    client = storage.Client()
    client.bucket(bucket_name).blob(blob_path).upload_from_filename('model.joblib')

if __name__ == '__main__':
    main()