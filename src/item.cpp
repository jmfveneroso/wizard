#include "item.hpp"

Item::Item(shared_ptr<AssetCatalog> asset_catalog) 
  : asset_catalog_(asset_catalog) {
}

void Item::ProcessItems() {
  vector<shared_ptr<GameObject>> extractables = asset_catalog_->GetExtractables();
  if (extractables.size() == 0 && respawn_countdown_ < 0) {
    respawn_countdown_ = 1000;
  }

  respawn_countdown_--;
  if (respawn_countdown_ == 0) {
    vec3 pos = vec3(11500, 26, 7165);
    shared_ptr<GameObject> item = asset_catalog_->CreateGameObjFromAsset(
      "rock", pos);

    pos = vec3(11600, 32, 7165);
    item = asset_catalog_->CreateGameObjFromAsset(
      "weeds", pos);
  }

  for (auto obj : extractables) {
    if (obj->status == STATUS_BEING_EXTRACTED) {
      obj->life -= 0.01;
      obj->quadratic *= 0.995;
    }

    if (obj->life > 0) continue;

    shared_ptr<GameAsset> asset = obj->GetAsset();
    if (!asset) continue;

    if (asset->name == "rock") {
      asset_catalog_->CreateParticleEffect(64, obj->position, 
        vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
      asset_catalog_->RemoveObject(obj);
  
      for (int i = 0; i < 5; i++) {
        std::normal_distribution<float> distribution(0.0, 10.0);
        float x = distribution(generator_);
        float y = distribution(generator_);
        shared_ptr<GameObject> item = asset_catalog_->CreateGameObjFromAsset(
          "small-rock", obj->position + vec3(x, 0, y));

        vec3 main_direction = vec3(0.0f, 1.0f, 0.0f);
        vec3 rand_direction = glm::vec3(
          (rand() % 2000 - 1000.0f) / 1000.0f,
          (rand() % 2000 - 1000.0f) / 1000.0f,
          (rand() % 2000 - 1000.0f) / 1000.0f
        );
        item->speed = main_direction + rand_direction * 5.0f;
      }
    }

    if (asset->name == "weeds") {
      asset_catalog_->CreateParticleEffect(64, obj->position, 
        vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
      asset_catalog_->RemoveObject(obj);
  
      for (int i = 0; i < 5; i++) {
        std::normal_distribution<float> distribution(0.0, 10.0);
        float x = distribution(generator_);
        float y = distribution(generator_);
        shared_ptr<GameObject> item = asset_catalog_->CreateGameObjFromAsset(
          "berry", obj->position + vec3(x, 0, y));

        vec3 main_direction = vec3(0.0f, 1.0f, 0.0f);
        vec3 rand_direction = glm::vec3(
          (rand() % 2000 - 1000.0f) / 1000.0f,
          (rand() % 2000 - 1000.0f) / 1000.0f,
          (rand() % 2000 - 1000.0f) / 1000.0f
        );
        item->speed = main_direction + rand_direction * 5.0f;
      }
    }
  }
}
