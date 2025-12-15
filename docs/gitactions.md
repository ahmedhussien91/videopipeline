1️⃣ إنشاء مشروع Google Cloud

افتح: https://console.cloud.google.com

من الأعلى → Select project → New Project

الاسم:

sdk-distribution


Create

2️⃣ تفعيل Cloud Storage API

داخل المشروع:

APIs & Services → Library

ابحث عن:

Cloud Storage


Enable

(غالبًا مفعّل افتراضيًا، بس تأكد)

3️⃣ إنشاء Bucket (خاص)

Storage → Buckets → Create

الإعدادات المقترحة:

Name:

my-private-sdk-bucket


Location type: Region

Region: أقرب لك (europe-west1 مثلًا)

Storage class: Standard

Access control: Uniform

Public access: Prevent

Create

✅ bucket private تمامًا

4️⃣ رفع ملف الـ SDK

من جهازك المحلي (بعد تثبيت gcloud):

gcloud init
gcloud auth login
gcloud config set project sdk-distribution

gsutil cp sdk.tar.gz gs://my-private-sdk-bucket/

5️⃣ إنشاء Service Account (لـ CI)

IAM & Admin → Service Accounts

Create service account:

Name:

github-actions-sdk


Grant role:

Storage Object Viewer

Finish

6️⃣ إنشاء Key (JSON)

افتح Service Account

Keys → Add key → Create new key

اختر:

JSON

نزّل الملف (مثلًا gcs-key.json)

⚠️ مهم: خزّنه بأمان، ده مفتاح كامل.

7️⃣ (خيار A – المفضل) استخدام Signed URL

ده أسهل وأسرع مع GitHub Actions.

توليد Signed URL (يدوي أو سكربت)

على جهازك:

gsutil signurl -d 24h gcs-key.json \
  gs://my-private-sdk-bucket/sdk.tar.gz


هيديك URL زي:

https://storage.googleapis.com/...

GitHub Actions (Download)
- name: Download SDK
  run: |
    curl -L "$SDK_URL" -o sdk.tar.gz


وتحط SDK_URL كـ:

GitHub Secret

أو Artifact

أو متغير في workflow generator

✅ لا OAuth
✅ curl فقط
