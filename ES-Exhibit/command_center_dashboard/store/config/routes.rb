Rails.application.routes.draw do
  get "messages/index"
  get "messages/show"
  namespace :api do
    namespace :v1 do
      resources :messages, only: [:create]
    end
  end

  resources :messages, only: [:index, :show]
  root "messages#index"
end
